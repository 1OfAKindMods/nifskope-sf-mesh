/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "glmesh.h"

#include "message.h"
#include "gl/controllers.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "io/material.h"
#include "io/nifstream.h"
#include "model/nifmodel.h"
#include "glview.h"

#include <QBuffer>
#include <QDebug>
#include <QSettings>

#include <QOpenGLFunctions>


//! @file glmesh.cpp Scene management for visible meshes such as NiTriShapes.

const char * NIMESH_ABORT = QT_TR_NOOP( "NiMesh rendering encountered unsupported types. Rendering may be broken." );

void Mesh::updateImpl( const NifModel * nif, const QModelIndex & index )
{
	Shape::updateImpl(nif, index);

	if ( index == iBlock ) {
		isLOD = nif->isNiBlock( iBlock, "BSLODTriShape" );
		if ( isLOD )
			emit nif->lodSliderChanged( true );

	} else if ( index == iData || index == iTangentData ) {
		needUpdateData = true;

	}
}

void Mesh::updateData( const NifModel * nif )
{
	resetSkinning();
	resetVertexData();
	if ( nif->checkVersion( 0x14050000, 0 ) && nif->blockInherits( iBlock, "NiMesh" ) )
		updateData_NiMesh( nif );
	else
		updateData_NiTriShape( nif );

	// Fill skinning and skeleton data
	resetSkeletonData();
	if ( iSkin.isValid() ) {
		isSkinned = true;

		iSkinData = nif->getBlockIndex( nif->getLink( iSkin, "Data" ), "NiSkinData" );

		iSkinPart = nif->getBlockIndex( nif->getLink( iSkin, "Skin Partition" ), "NiSkinPartition" );
		if ( !iSkinPart.isValid() && iSkinData.isValid() ) {
			// nif versions < 10.2.0.0 have skin partition linked in the skin data block
			iSkinPart = nif->getBlockIndex( nif->getLink( iSkinData, "Skin Partition" ), "NiSkinPartition" );
		}

		skeletonRoot = nif->getLink( iSkin, "Skeleton Root" );
		skeletonTrans = Transform( nif, iSkinData );

		bones = nif->getLinkArray( iSkin, "Bones" );

		QModelIndex idxBones = nif->getIndex( iSkinData, "Bone List" );
		if ( idxBones.isValid() ) {
			int nTotalBones = bones.size();
			int nBoneList = nif->rowCount( idxBones );
#if 0
			// TODO: Ignore weights listed in NiSkinData if NiSkinPartition exists
			int vcnt = ( !iSkinPart.isValid() ? numVerts : 0 );
#endif
			for ( int b = 0; b < nBoneList && b < nTotalBones; b++ )
				boneData.append( BoneData( nif, nif->getIndex( idxBones, b ), bones[b] ) );
		}

		if ( iSkinPart.isValid() ) {
			QModelIndex idx = nif->getIndex( iSkinPart, "Partitions" );

			uint numTris = 0;
			uint numStrips = 0;
			for ( int i = 0; i < nif->rowCount( idx ) && idx.isValid(); i++ ) {
				partitions.append( SkinPartition( nif, nif->getIndex( idx, i ) ) );
				numTris += partitions[i].triangles.size();
				numStrips += partitions[i].tristrips.size();
			}

			triangles.clear();
			tristripOffsets.clear();

			triangles.reserve( numTris );
			tristripOffsets.reserve( numStrips );

			for ( const SkinPartition& part : partitions ) {
				triangles << part.getRemappedTriangles();
				auto	tristrips = part.getRemappedTristrips();
				// convert triangle strips to triangles
				for ( const auto & i : tristrips )
					Shape::convertTriangleStrip( i.constData(), size_t( i.size() ) );
			}
		}
	}

	removeInvalidIndices();
	updateLodLevel();
}

void Mesh::updateData_NiMesh( const NifModel * nif )
{
	iData = nif->getIndex( iBlock, "Datastreams" );
	if ( !iData.isValid() )
		return;
	int nTotalStreams = nif->rowCount( iData );

	// All the semantics used by this mesh
	NiMesh::SemanticFlags semFlags = NiMesh::HAS_NONE;

	// Loop over the data once for initial setup and validation
	// and build the semantic-index maps for each datastream's components
	using CompSemIdxMap = QVector<QPair<NiMesh::Semantic, uint>>;
	QVector<CompSemIdxMap> compSemanticIndexMaps;
	for ( int i = 0; i < nTotalStreams; i++ ) {
		auto iStreamEntry = nif->getIndex( iData, i );

		auto stream = nif->getLink( iStreamEntry, "Stream" );
		auto iDataStream = nif->getBlockIndex( stream );

		auto usage = NiMesh::DataStreamUsage( nif->get<uint>( iDataStream, "Usage" ) );
		auto access = nif->get<uint>( iDataStream, "Access" );

		// Invalid Usage and Access, abort
		if ( usage == access && access == 0 )
			return;

		// For each datastream, store the semantic and the index (used for E_TEXCOORD)
		auto iComponentSemantics = nif->getIndex( iStreamEntry, "Component Semantics" );
		uint numComponents = nif->get<uint>( iStreamEntry, "Num Components" );
		CompSemIdxMap compSemanticIndexMap;
		for ( uint j = 0; j < numComponents; j++ ) {
			auto iComponentEntry = nif->getIndex( iComponentSemantics, j );

			auto name = nif->get<QString>( iComponentEntry, "Name" );
			auto sem = NiMesh::semanticStrings.value( name );
			uint idx = nif->get<uint>( iComponentEntry, "Index" );
			compSemanticIndexMap.insert( j, {sem, idx} );

			// Create UV stubs for multi-coord systems
			if ( sem == NiMesh::E_TEXCOORD )
				coords.append( TexCoords() );

			// Assure Index datastream is first and Usage is correct
			bool invalidIndex = false;
			if ( (sem == NiMesh::E_INDEX && (i != 0 || usage != NiMesh::USAGE_VERTEX_INDEX))
					|| (usage == NiMesh::USAGE_VERTEX_INDEX && (i != 0 || sem != NiMesh::E_INDEX)) )
				invalidIndex = true;

			if ( invalidIndex ) {
				Message::append( tr( NIMESH_ABORT ),
									tr( "[%1] NifSkope requires 'INDEX' datastream be first, with Usage type 'USAGE_VERTEX_INDEX'." )
									.arg( stream ),
									QMessageBox::Warning );
				return;
			}

			semFlags = NiMesh::SemanticFlags(semFlags | (1 << sem));
		}

		compSemanticIndexMaps << compSemanticIndexMap;
	}

	// This NiMesh does not have vertices, abort
	if ( !(semFlags & NiMesh::HAS_POSITION || semFlags & NiMesh::HAS_POSITION_BP) )
		return;

	// The number of triangle indices across the submeshes for this NiMesh
	quint32 totalIndices = 0;
	QVector<quint16> indices;
	// The highest triangle index value in this NiMesh
	quint32 maxIndex = 0;
	// The Nth component after ignoring DataStreamUsage > 1
	int compIdx = 0;
	for ( int i = 0; i < nTotalStreams; i++ ) {
		// TODO: For now, submeshes are not actually used and the regions are
		// filled in order for each data stream.
		// Submeshes may be required if total index values exceed USHRT_MAX
		auto iStreamEntry = nif->getIndex( iData, i );

		QMap<ushort, ushort> submeshMap;
		ushort numSubmeshes = nif->get<ushort>( iStreamEntry, "Num Submeshes" );
		auto iSubmeshMap = nif->getIndex( iStreamEntry, "Submesh To Region Map" );
		for ( ushort j = 0; j < numSubmeshes; j++ )
			submeshMap.insert( j, nif->get<ushort>( nif->getIndex( iSubmeshMap, j ) ) );

		// Get the datastream
		quint32 stream = nif->getLink( iStreamEntry, "Stream" );
		auto iDataStream = nif->getBlockIndex( stream );

		auto usage = NiMesh::DataStreamUsage(nif->get<uint>( iDataStream, "Usage" ));
		// Only process USAGE_VERTEX and USAGE_VERTEX_INDEX
		if ( usage > NiMesh::USAGE_VERTEX )
			continue;

		// Datastream can be split into multiple regions
		// Each region has a Start Index which is added as an offset to the index read from the stream
		QVector<QPair<quint32, quint32>> regions;
		quint32 numIndices = 0;
		auto iRegions = nif->getIndex( iDataStream, "Regions" );
		if ( iRegions.isValid() ) {
			quint32 numRegions = nif->get<quint32>( iDataStream, "Num Regions" );
			for ( quint32 j = 0; j < numRegions; j++ ) {
				auto iRegionEntry = nif->getIndex( iRegions, j );
				regions.append( { nif->get<quint32>( iRegionEntry, "Start Index" ), nif->get<quint32>( iRegionEntry, "Num Indices" ) } );

				numIndices += regions[j].second;
			}
		}

		if ( usage == NiMesh::USAGE_VERTEX_INDEX ) {
			totalIndices = numIndices;
			// RESERVE not RESIZE
			indices.reserve( totalIndices );
		} else if ( compIdx == 1 ) {
			// Indices should be built already
			if ( indices.size() != qsizetype( totalIndices ) )
				return;

			quint32 maxSize = maxIndex + 1;
			// RESIZE
			verts.resize( maxSize );
			norms.resize( maxSize );
			tangents.resize( maxSize );
			bitangents.resize( maxSize );
			colors.resize( maxSize );
			boneData.resize( maxSize );
			if ( coords.size() == 0 )
				coords.resize( 1 );

			for ( auto & c : coords )
				c.resize( maxSize );
		}

		// Get the format of each component
		QVector<NiMesh::DataStreamFormat> datastreamFormats;
		uint numStreamComponents = nif->get<uint>( iDataStream, "Num Components" );
		auto iComponentFormats = nif->getIndex( iDataStream, "Component Formats" );
		for ( uint j = 0; j < numStreamComponents; j++ ) {
			auto format = nif->get<uint>( nif->getIndex( iComponentFormats, j ) );
			datastreamFormats.append( NiMesh::DataStreamFormat(format) );
		}

		Q_ASSERT( compSemanticIndexMaps[i].size() == qsizetype(numStreamComponents) );

		auto tempMdl = std::make_unique<NifModel>( this );

		QByteArray streamData = nif->get<QByteArray>( nif->getIndex( nif->getIndex( iDataStream, "Data" ), 0 ) );
		QBuffer streamBuffer( &streamData );
		streamBuffer.open( QIODevice::ReadOnly );

		NifIStream tempInput( tempMdl.get(), &streamBuffer );
		NifValue tempValue;

		bool abort = false;
		for ( const auto & r : regions ) for ( uint j = 0; j < r.second; j++ ) {
			auto off = r.first;
			Q_ASSERT( totalIndices >= off + j );
			for ( uint k = 0; k < numStreamComponents; k++ ) {
				auto typeK = datastreamFormats[k];
				int typeLength = ( (typeK & 0x000F0000) >> 0x10 );

				switch ( (typeK & 0x00000FF0) >> 0x04 ) {
				case 0x10:
					tempValue.changeType( NifValue::tByte );
					break;
				case 0x11:
					if ( typeK == NiMesh::F_NORMUINT8_4 )
						tempValue.changeType( NifValue::tByteColor4 );
					typeLength = 1;
					break;
				case 0x13:
					if ( typeK == NiMesh::F_NORMUINT8_4_BGRA )
						tempValue.changeType( NifValue::tByteColor4 );
					typeLength = 1;
					break;
				case 0x21:
					tempValue.changeType( NifValue::tShort );
					break;
				case 0x23:
					if ( typeLength == 3 )
						tempValue.changeType( NifValue::tHalfVector3 );
					else if ( typeLength == 2 )
						tempValue.changeType( NifValue::tHalfVector2 );
					else if ( typeLength == 1 )
						tempValue.changeType( NifValue::tHfloat );

					typeLength = 1;

					break;
				case 0x42:
					tempValue.changeType( NifValue::tInt );
					break;
				case 0x43:
					if ( typeLength == 3 )
						tempValue.changeType( NifValue::tVector3 );
					else if ( typeLength == 2 )
						tempValue.changeType( NifValue::tVector2 );
					else if ( typeLength == 4 )
						tempValue.changeType( NifValue::tVector4 );
					else if ( typeLength == 1 )
						tempValue.changeType( NifValue::tFloat );

					typeLength = 1;

					break;
				}

				for ( int l = 0; l < typeLength; l++ ) {
					tempInput.read( tempValue );
				}

				auto compType = compSemanticIndexMaps[i].value( k ).first;
				switch ( typeK )
				{
				case NiMesh::F_FLOAT32_3:
				case NiMesh::F_FLOAT16_3:
					Q_ASSERT( usage == NiMesh::USAGE_VERTEX );
					switch ( compType ) {
					case NiMesh::E_POSITION:
					case NiMesh::E_POSITION_BP:
						verts[j + off] = tempValue.get<Vector3>( nif, nullptr );
						break;
					case NiMesh::E_NORMAL:
					case NiMesh::E_NORMAL_BP:
						norms[j + off] = tempValue.get<Vector3>( nif, nullptr );
						break;
					case NiMesh::E_TANGENT:
					case NiMesh::E_TANGENT_BP:
						tangents[j + off] = tempValue.get<Vector3>( nif, nullptr );
						break;
					case NiMesh::E_BINORMAL:
					case NiMesh::E_BINORMAL_BP:
						bitangents[j + off] = tempValue.get<Vector3>( nif, nullptr );
						break;
					default:
						break;
					}
					break;
				case NiMesh::F_UINT16_1:
					if ( compType == NiMesh::E_INDEX ) {
						Q_ASSERT( usage == NiMesh::USAGE_VERTEX_INDEX );
						// TODO: The total index value across all submeshes
						// is likely allowed to exceed USHRT_MAX.
						// For now limit the index.
						quint32 ind = tempValue.get<quint16>( nif, nullptr ) + off;
						if ( ind > 0xFFFF )
							qDebug() << QString( "[%1] %2" ).arg( stream ).arg( ind );

						ind = std::min( ind, (quint32)0xFFFF );

						// Store the highest index
						if ( ind > maxIndex )
							maxIndex = ind;

						indices.append( (quint16)ind );
					}
					break;
				case NiMesh::F_FLOAT32_2:
				case NiMesh::F_FLOAT16_2:
					Q_ASSERT( usage == NiMesh::USAGE_VERTEX );
					if ( compType == NiMesh::E_TEXCOORD ) {
						quint32 coordSet = compSemanticIndexMaps[i].value( k ).second;
						Q_ASSERT( coords.size() > qsizetype(coordSet) );
						coords[coordSet][j + off] = tempValue.get<Vector2>( nif, nullptr );
					}
					break;
				case NiMesh::F_UINT8_4:
					// BLENDINDICES, do nothing for now
					break;
				case NiMesh::F_NORMUINT8_4:
					Q_ASSERT( usage == NiMesh::USAGE_VERTEX );
					if ( compType == NiMesh::E_COLOR )
						colors[j + off] = tempValue.get<ByteColor4>( nif, nullptr );
					break;
				case NiMesh::F_NORMUINT8_4_BGRA:
					Q_ASSERT( usage == NiMesh::USAGE_VERTEX );
					if ( compType == NiMesh::E_COLOR ) {
						// Swizzle BGRA -> RGBA
						auto c = tempValue.get<ByteColor4>( nif, nullptr );
						colors[j + off] = {c[2], c[1], c[0], c[3]};
					}
					break;
				default:
					Message::append( tr( NIMESH_ABORT ), tr( "[%1] Unsupported Component: %2" ).arg( stream )
										.arg( NifValue::enumOptionName( "ComponentFormat", typeK ) ),
										QMessageBox::Warning );
					abort = true;
					break;
				}

				if ( abort == true )
					break;
			}
		}

		// Clear is extremely expensive. Must be outside of loop
		tempMdl->clear();

		compIdx++;
	}

	// Clear unused vertex attributes
	//	Note: Do not clear normals as this breaks fixed function for some reason
	// TODO (Gavrant): figure out why clearing normals "breaks fixed function for some reason"
	if ( !(semFlags & NiMesh::HAS_BINORMAL) )
		bitangents.clear();
	if ( !(semFlags & NiMesh::HAS_TANGENT) )
		tangents.clear();
	if ( !(semFlags & NiMesh::HAS_COLOR) )
		colors.clear();
	if ( !(semFlags & NiMesh::HAS_BLENDINDICES) || !(semFlags & NiMesh::HAS_BLENDWEIGHT) )
		boneData.clear();

	Q_ASSERT( verts.size() == qsizetype(maxIndex + 1) );
	Q_ASSERT( indices.size() == qsizetype(totalIndices) );

	// Make geometry
	triangles.resize( indices.size() / 3 );
	auto meshPrimitiveType = nif->get<uint>( iBlock, "Primitive Type" );
	switch ( meshPrimitiveType ) {
	case NiMesh::PRIMITIVE_TRIANGLES:
		for ( int k = 0, t = 0; k < indices.size(); k += 3, t++ )
			triangles[t] = { indices[k], indices[k + 1], indices[k + 2] };
		break;
	case NiMesh::PRIMITIVE_TRISTRIPS:
	case NiMesh::PRIMITIVE_LINES:
	case NiMesh::PRIMITIVE_LINESTRIPS:
	case NiMesh::PRIMITIVE_QUADS:
	case NiMesh::PRIMITIVE_POINTS:
		Message::append( tr( NIMESH_ABORT ), tr( "[%1] Unsupported Primitive: %2" )
							.arg( nif->getBlockNumber( iBlock ) )
							.arg( NifValue::enumOptionName( "MeshPrimitiveType", meshPrimitiveType ) ),
							QMessageBox::Warning
		);
		break;
	}
}

void Mesh::updateData_NiTriShape( const NifModel * nif )
{
	// Find iData and iSkin blocks among the children
	for ( auto childLink : nif->getChildLinks( id() ) ) {
		QModelIndex iChild = nif->getBlockIndex( childLink );
		if ( !iChild.isValid() )
			continue;

		if ( nif->blockInherits( iChild, "NiTriShapeData" ) || nif->blockInherits( iChild, "NiTriStripsData" ) ) {
			if ( !iData.isValid() ) {
				iData = iChild;
			} else if ( iData != iChild ) {
				Message::append( tr( "Warnings were generated while updating meshes." ),
					tr( "Block %1 has multiple data blocks" ).arg( id() )
				);
			}
		} else if ( nif->blockInherits( iChild, "NiSkinInstance" ) ) {
			if ( !iSkin.isValid() ) {
				iSkin = iChild;
			} else if ( iSkin != iChild ) {
				Message::append( tr( "Warnings were generated while updating meshes." ),
					tr( "Block %1 has multiple skin instances" ).arg( id() )
				);
			}
		}
	}
	if ( !iData.isValid() )
		return;

	// Fill vertex data
	verts = nif->getArray<Vector3>( iData, "Vertices" );
	qsizetype numVerts = verts.size();

	norms = nif->getArray<Vector3>( iData, "Normals" );
	if ( norms.size() < numVerts )
		norms.clear();

	colors = nif->getArray<Color4>( iData, "Vertex Colors" );
	if ( colors.size() < numVerts )
		colors.clear();
	// Detect if "Has Vertex Colors" is set to Yes in NiTriShape
	//	Used to compare against SLSF2_Vertex_Colors
	hasVertexColors = (colors.size() > 0);

	tangents   = nif->getArray<Vector3>( iData, "Tangents" );
	bitangents = nif->getArray<Vector3>( iData, "Bitangents" );

	QModelIndex iExtraData = nif->getIndex( iBlock, "Extra Data List" );
	if ( iExtraData.isValid() ) {
		int nExtra = nif->rowCount( iExtraData );
		for ( int e = 0; e < nExtra; e++ ) {
			QModelIndex iExtra = nif->getBlockIndex( nif->getLink( nif->getIndex( iExtraData, e ) ), "NiBinaryExtraData" );
			if ( nif->get<QString>( iExtra, "Name" ) == "Tangent space (binormal & tangent vectors)" ) {
				iTangentData = iExtra;
				QByteArray data = nif->get<QByteArray>( iExtra, "Binary Data" );
				if ( data.size() == numVerts * 4 * 3 * 2 ) {
					tangents.resize( numVerts );
					bitangents.resize( numVerts );
					Vector3 * t = (Vector3 *)data.data();

					for ( int c = 0; c < numVerts; c++ )
						tangents[c] = *t++;

					for ( int c = 0; c < numVerts; c++ )
						bitangents[c] = *t++;
				}
			}
		}
	}

	coords.clear();
	QModelIndex iUVSets = nif->getIndex( iData, "UV Sets" );
	if ( iUVSets.isValid() ) {
		int nSets = nif->rowCount( iUVSets );
		for ( int r = 0; r < nSets; r++ ) {
			TexCoords tc = nif->getArray<Vector2>( nif->getIndex( iUVSets, r ) );
			if ( tc.size() < numVerts )
				tc.clear();
			coords.append( tc );
		}
	}

	// Fill triangle/strips data
	auto dataName = nif->itemName( iData );
	if ( dataName == "NiTriShapeData" ) {
		// check indexes
		// TODO: check other indexes as well
		// TODO (Gavrant): test this!
		QVector<Triangle> dataTris = nif->getArray<Triangle>( iData, "Triangles" );
		int nDataTris = dataTris.size();

		for ( int i = 0; i < nDataTris; i++ ) {
			Triangle t = dataTris[i];
			if ( t[0] < numVerts && t[1] < numVerts && t[2] < numVerts )
				triangles.append(t);
		}

		int diff = nDataTris - triangles.size();
		if ( diff > 0 ) {
			int block_idx = nif->getBlockNumber( nif->getIndex( iData, "Triangles" ) );
			Message::append( tr( "Warnings were generated while rendering mesh." ),
				tr( "Block %1: %2 invalid indices in NiTriShapeData.Triangles" ).arg( block_idx ).arg( diff )
			);
		}
	} else if ( dataName == "NiTriStripsData" ) {
		QModelIndex points = nif->getIndex( iData, "Points" );
		if ( points.isValid() ) {
			int nStrips = nif->rowCount( points );
			// convert triangle strips to triangles
			for ( int r = 0; r < nStrips; r++ ) {
				auto	tristrip = nif->getArray<quint16>( nif->getIndex( points, r ) );
				Shape::convertTriangleStrip( tristrip.constData(), size_t( tristrip.size() ) );
			}
		} else {
			Message::append( tr( "Warnings were generated while rendering mesh." ),
				tr( "Block %1: Invalid 'Points' array in %2" )
				.arg( nif->getBlockNumber( iData ) )
				.arg( dataName )
			);
		}
	}
}

QModelIndex Mesh::vertexAt( int idx ) const
{
	auto nif = NifModel::fromIndex( iBlock );
	if ( !nif )
		return QModelIndex();

	auto iVertexData = nif->getIndex( iData, "Vertices" );
	auto iVertex = nif->getIndex( iVertexData, idx );

	return iVertex;
}

bool compareTriangles( const QPair<int, float> & tri1, const QPair<int, float> & tri2 )
{
	return ( tri1.second < tri2.second );
}

void Mesh::transformShapes()
{
	if ( isHidden() )
		return;

	Node::transformShapes();

	transformRigid = true;

	if ( isSkinned && ( boneData.size() || partitions.size() ) && scene->hasOption(Scene::DoSkinning) ) {
		transformRigid = false;

		Node * root = findParent( skeletonRoot );

		boundSphere = BoundSphere();

		if ( partitions.size() ) {
			for ( const SkinPartition& part : partitions ) {
				QVector<Transform> boneTrans( part.boneMap.size() );

				for ( int t = 0; t < boneTrans.size(); t++ ) {
					Node * bone = root ? root->findChild( bones.value( part.boneMap[t] ) ) : 0;
					boneTrans[ t ] = scene->view;

					if ( bone )
						boneTrans[ t ] = boneTrans[ t ] * bone->localTrans( skeletonRoot ) * boneData.value( part.boneMap[t] ).trans;

					//if ( bone ) boneTrans[ t ] = bone->viewTrans() * boneData.value( part.boneMap[t] ).trans;
				}
			}
		} else {
			int x = 0;
			for ( const BoneData& bw : boneData ) {
				Transform trans = viewTrans() * skeletonTrans;
				Node * bone = root ? root->findChild( bw.bone ) : nullptr;

				if ( bone ) {
					trans = trans * bone->localTrans( skeletonRoot ) * bw.trans;
					boneData[x].tcenter = bone->viewTrans() * bw.center;
				}
				x++;
			}
		}

		boundSphere.applyInv( worldTrans() );
		needUpdateBounds = false;
	}
}

BoundSphere Mesh::bounds() const
{
	if ( needUpdateBounds ) {
		needUpdateBounds = false;
		boundSphere = BoundSphere( verts );
	}

	return worldTrans() * boundSphere;
}

void Mesh::drawShapes( NodeList * secondPass )
{
	if ( isHidden() )
		return;

	// TODO: Only run this if BSXFlags has "EditorMarkers present" flag
	if ( !scene->hasOption(Scene::ShowMarkers) && name.startsWith( "EditorMarker" ) )
		return;

	// Draw translucent meshes in second pass
	if ( secondPass && drawInSecondPass ) {
		secondPass->add( this );
		return;
	}

	auto	nif = NifModel::fromIndex( iBlock );
	auto	context = scene->renderer;

	if ( !nif || !context || !bindShape() )
		return;

	glDisable( GL_FRAMEBUFFER_SRGB );

	if ( scene->selecting && scene->isSelModeVertex() ) [[unlikely]] {
		drawVerts();
		return;
	}

	// TODO: Option to hide Refraction and other post effects

	//if ( !scene->selecting ) {
	//	qDebug() << viewTrans().translation;
		//qDebug() << Vector3( nif->get<Vector4>( iBlock, "Translation" ) );
	//}

	// Debug axes
	//drawAxes(Vector3(), 35.0);

	// Render polygon fill slightly behind alpha transparency and wireframe
	glEnable( GL_POLYGON_OFFSET_FILL );
	if ( drawInSecondPass )
		glPolygonOffset( 0.5f, 1.0f );
	else
		glPolygonOffset( 1.0f, 2.0f );

	if ( !scene->selecting ) [[likely]] {
		// TODO: Hotspot.  See about optimizing this.
		shader = context->setupProgram( this, shader );

	} else {
		auto	prog = context->useProgram( "selection.prog" );
		if ( prog ) {
			setUniforms( prog );
			prog->uni1i( "selectionFlags", 0x0001 );
			prog->uni1i( "selectionParam", ( scene->isSelModeObject() ? nodeId : -1 ) );
		}
	}

	if ( isDoubleSided ) {
		glDisable( GL_CULL_FACE );
	}

	qsizetype	numTriangles = std::clamp< qsizetype >( lodTriangleCount, 0, triangles.size() );

	// render the triangles
	if ( numTriangles > 0 )
		context->fn->glDrawElements( GL_TRIANGLES, GLsizei( numTriangles ) * 3, GL_UNSIGNED_SHORT, (void *) 0 );

	if ( isDoubleSided ) {
		glEnable( GL_CULL_FACE );
	}

	glDisable( GL_POLYGON_OFFSET_FILL );
}

void Mesh::drawVerts() const
{
	int	vertexSelected = -1;

	// Highlight selected vertex
	if ( !scene->selecting && iData == scene->currentBlock ) {
		auto idx = scene->currentIndex;
		if ( idx.data( NifSkopeDisplayRole ).toString() == "Vertices" )
			vertexSelected = idx.row();
	}

	Shape::drawVerts( GLView::Settings::vertexSelectPointSize, vertexSelected );
}

void Mesh::drawSelection() const
{
	if ( !scene->isSelModeVertex() ) {
		if ( scene->hasOption(Scene::ShowNodes) )
			Node::drawSelection();

		if ( !scene->isSelModeObject() )
			return;
	}

	auto nif = scene->nifModel;
	if ( isHidden() || !nif )
		return;
	auto idx = scene->currentIndex;
	auto blk = scene->currentBlock;

	if ( blk != iBlock && blk != iData && blk != iSkinPart && blk != iSkinData
		&& ( !iTangentData.isValid() || blk != iTangentData ) && !scene->isSelModeVertex() )
	{
		return;
	}

	auto	context = scene->renderer;
	if ( !( context && bindShape() ) )
		return;

	glDepthFunc( GL_LEQUAL );
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDisable( GL_CULL_FACE );
	glDisable( GL_FRAMEBUFFER_SRGB );

	if ( scene->isSelModeVertex() ) {
		drawVerts();
		return;
	}

	glEnable( GL_BLEND );
	context->fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	glEnable( GL_POLYGON_OFFSET_FILL );
	glEnable( GL_POLYGON_OFFSET_LINE );
	glEnable( GL_POLYGON_OFFSET_POINT );
	glPolygonOffset( -1.0f, -2.0f );

	QString n;
	int i = -1;

	if ( blk == iBlock || idx == iData ) {
		n = "Faces";
	} else if ( blk == iData || blk == iSkinPart ) {
		n = idx.data( NifSkopeDisplayRole ).toString();

		QModelIndex iParent = idx.parent();
		if ( iParent.isValid() && iParent != iData ) {
			n = iParent.data( NifSkopeDisplayRole ).toString();
			i = idx.row();
		}
	} else if ( blk == iTangentData ) {
		n = "TSpace";
	} else {
		n = idx.data( NifSkopeDisplayRole ).toString();
	}

	if ( n == "Vertices" || n == "Vertex Colors" || n == "UV Sets" ) {
		Shape::drawVerts( GLView::Settings::vertexPointSize, i );

	} else if ( n == "Normals" || n == "Tangents" || n == "Bitangents" || n == "TSpace" ) {
		int	btnMask = ( n == "Bitangents" ? 0x01 : ( n == "Tangents" ? 0x02 : 0x04 ) );
		Shape::drawVerts( GLView::Settings::tbnPointSize, i );
		float	normalScale = std::max< float >( bounds().radius / ( btnMask == 0x04 ? 8.0f : 16.0f ), 0.25f );
		Shape::drawNormals( btnMask, i, normalScale * viewTrans().scale );
	}

	if ( n == "Points" ) {
		// TODO: implement this
#if 0
		glBegin( GL_POINTS );
		auto nif = NifModel::fromIndex( iData );
		QModelIndex points = nif->getIndex( iData, "Points" );

		if ( points.isValid() ) {
			for ( int j = 0; j < nif->rowCount( points ); j++ ) {
				QModelIndex iPoints = nif->getIndex( points, j );

				for ( int k = 0; k < nif->rowCount( iPoints ); k++ ) {
					glVertex( verts.value( nif->get<quint16>( nif->getIndex( iPoints, k ) ) ) );
				}
			}
		}

		glEnd();

		if ( i >= 0 ) {
			glDepthFunc( GL_ALWAYS );
			glHighlightColor();
			glBegin( GL_POINTS );
			QModelIndex iPoints = nif->getIndex( points, i );

			if ( nif->isArray( idx ) ) {
				for ( int j = 0; j < nif->rowCount( iPoints ); j++ ) {
					glVertex( verts.value( nif->get<quint16>( nif->getIndex( iPoints, j ) ) ) );
				}
			} else {
				iPoints = idx.parent();
				glVertex( verts.value( nif->get<quint16>( nif->getIndex( iPoints, i ) ) ) );
			}

			glEnd();
		}
#endif
	}

	if ( n == "Faces" || n == "Triangles" ) {
		Shape::drawWireframe( scene->wireframeColor );

		if ( i >= 0 && i < triangles.size() )
			Shape::drawTriangles( i, 1, scene->highlightColor );
	}

	if ( n == "Strips" || n == "Strip Lengths" ) {
		Shape::drawWireframe( scene->wireframeColor );

		if ( i >= 0 && i < tristripOffsets.size() )
			Shape::drawTriangles( tristripOffsets[i].first, tristripOffsets[i].second, scene->highlightColor );
	}

	if ( n == "Partitions" ) {

		for ( int c = 0; c < partitions.size(); c++ ) {
			// TODO: implement this
#if 0
			if ( c == i )
				glHighlightColor();
			else
				glNormalColor();

			QVector<int> vmap = partitions[c].vertexMap;

			for ( const Triangle& tri : partitions[c].triangles ) {
				glBegin( GL_LINE_STRIP );
				glVertex( verts.value( vmap.value( tri.v1() ) ) );
				glVertex( verts.value( vmap.value( tri.v2() ) ) );
				glVertex( verts.value( vmap.value( tri.v3() ) ) );
				glVertex( verts.value( vmap.value( tri.v1() ) ) );
				glEnd();
			}
			for ( const TriStrip& strip : partitions[c].tristrips ) {
				quint16 a = vmap.value( strip.value( 0 ) );
				quint16 b = vmap.value( strip.value( 1 ) );

				for ( int v = 2; v < strip.size(); v++ ) {
					quint16 c = vmap.value( strip[v] );

					if ( a != b && b != c && c != a ) {
						glBegin( GL_LINE_STRIP );
						glVertex( verts.value( a ) );
						glVertex( verts.value( b ) );
						glVertex( verts.value( c ) );
						glVertex( verts.value( a ) );
						glEnd();
					}

					a = b;
					b = c;
				}
			}
#endif
		}
	}

	if ( n == "Bone List" ) {
		if ( nif->isArray( idx ) ) {
			for ( int i = 0; i < nif->rowCount( idx ); i++ )
				boneSphere( nif, nif->getIndex( idx, i ) );
		} else {
			boneSphere( nif, idx );
		}
	} else if ( n == "Bounding Sphere" ) {
		BoundSphere	sph( nif, idx );
		if ( sph.radius > 0.0f )
			Shape::drawBoundingSphere( sph, FloatVector4( 1.0f, 1.0f, 1.0f, 0.33f ) );
	}

	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_POLYGON_OFFSET_LINE );
	glDisable( GL_POLYGON_OFFSET_POINT );
}

QString Mesh::textStats() const
{
	QString	tmp = Node::textStats();
	if ( shader ) {
		tmp.append( QLatin1StringView( "\nshader: " ) );
		tmp.append( QString::fromUtf8( shader->name.data(), qsizetype( shader->name.length() ) ) );
	}
	tmp.append( QChar('\n') );
	return tmp;
}

void Mesh::updateLodLevel()
{
	auto	nif = scene->nifModel;
	qsizetype	numTriangles = triangles.size();
	if ( isLOD && nif && numTriangles > 0 ) {
		auto lod0 = nif->get<uint>( iBlock, "LOD0 Size" );
		auto lod1 = nif->get<uint>( iBlock, "LOD1 Size" );
		auto lod2 = nif->get<uint>( iBlock, "LOD2 Size" );

		// If Level2, render all
		// If Level1, also render Level0
		numTriangles = 0;
		switch ( scene->lodLevel ) {
		case Scene::Level0:
			numTriangles = qsizetype( lod2 );
			[[fallthrough]];
		case Scene::Level1:
			numTriangles += qsizetype( lod1 );
			[[fallthrough]];
		case Scene::Level2:
		default:
			numTriangles += qsizetype( lod0 );
			break;
		}
		numTriangles = std::clamp< qsizetype >( numTriangles, 0, triangles.size() );
	}
	lodTriangleCount = numTriangles;
}
