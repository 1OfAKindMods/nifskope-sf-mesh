#include "BSMesh.h"
#include "message.h"
#include "gl/controllers.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "io/nifstream.h"
#include "model/nifmodel.h"
#include "glview.h"

#include <QDir>
#include <QBuffer>


BSMesh::BSMesh(Scene* s, const QModelIndex& iBlock) : Shape(s, iBlock)
{
}

void BSMesh::transformShapes()
{
	if ( isHidden() )
		return;

	if ( !( isSkinned && scene->hasOption(Scene::DoSkinning) ) ) [[likely]] {
		transformRigid = true;
		return;
	}

	updateBoneTransforms();
}

void BSMesh::drawShapes( NodeList * secondPass )
{
	if ( isHidden() || ( !scene->hasOption(Scene::ShowMarkers) && name.contains(QLatin1StringView("EditorMarker")) ) )
		return;

	// Draw translucent meshes in second pass
	if ( secondPass && drawInSecondPass ) {
		secondPass->add(this);
		return;
	}

	auto nif = NifModel::fromIndex(iBlock);
	if ( lodLevel != scene->lodLevel ) {
		lodLevel = scene->lodLevel;
		updateData(nif);
	}

	if ( !bindShape() )
		return;

	if ( scene->selecting && scene->isSelModeVertex() ) [[unlikely]] {
		glDisable( GL_FRAMEBUFFER_SRGB );
		drawVerts();
		return;
	}

	glEnable( GL_POLYGON_OFFSET_FILL );
	if ( drawInSecondPass )
		glPolygonOffset( 0.5f, 1.0f );
	else
		glPolygonOffset( 1.0f, 2.0f );

	auto	context = scene->renderer;

	if ( !scene->selecting ) [[likely]] {
		glEnable( GL_FRAMEBUFFER_SRGB );
		shader = context->setupProgram( this, shader );

	} else {
		glDisable( GL_FRAMEBUFFER_SRGB );

		auto	prog = context->useProgram( "selection.prog" );
		if ( prog ) {
			setUniforms( prog );
			prog->uni1i( "selectionFlags", 0x0001 );
			prog->uni1i( "selectionParam", ( scene->isSelModeObject() ? nodeId : -1 ) );
		}
	}

	context->fn->glDrawElements( GL_TRIANGLES, GLsizei( triangles.size() * 3 ), GL_UNSIGNED_SHORT, (void *) 0 );

	glDisable( GL_POLYGON_OFFSET_FILL );
}

void BSMesh::drawSelection() const
{
	if ( !scene->isSelModeVertex() ) {
		if ( scene->hasOption(Scene::ShowNodes) )
			Node::drawSelection();

		if ( !( scene->isSelModeObject() && scene->currentBlock == iBlock ) )
			return;
	}

	auto &	idx = scene->currentIndex;
	auto	nif = scene->nifModel;
	auto	context = scene->renderer;
	if ( isHidden() || !( nif && context && bindShape() ) )
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

	// Name of this index
	auto n = idx.data( NifSkopeDisplayRole ).toString();
	// Name of this index's parent
	auto p = idx.parent().data( NifSkopeDisplayRole ).toString();

	qsizetype	numTriangles = triangles.size();

	if ( n == "Bounding Sphere" ) {
		auto sph = BoundSphere( nif, idx );
		if ( sph.radius > 0.0f )
			Shape::drawBoundingSphere( sph, FloatVector4( 1.0f, 1.0f, 1.0f, 0.33f ) );
	} else if ( n == "Bounding Box" ) {
		const NifItem *	boundsItem = nif->getItem( idx );
		Vector3	boundsCenter, boundsDims;
		if ( boundsItem ) {
			boundsCenter = nif->get<Vector3>( boundsItem->child( 0 ) );
			boundsDims = nif->get<Vector3>( boundsItem->child( 1 ) );
		}
		float	minVal = std::min( boundsDims[0], std::min( boundsDims[1], boundsDims[2] ) );
		float	maxVal = std::max( boundsDims[0], std::max( boundsDims[1], boundsDims[2] ) );
		if ( minVal > 0.0f && maxVal < 2.1e9f )
			Shape::drawBoundingBox( boundsCenter, boundsDims, FloatVector4( 1.0f, 1.0f, 1.0f, 0.33f ) );
	} else if ( n == "Vertices" || n == "UVs" || n == "UVs 2" || n == "Vertex Colors" || n == "Weights" ) {
		int	s = -1;
		if ( n == p && ( s = idx.row() ) >= 0 ) {
			if ( n == "Weights" ) {
				int	weightsPerVertex = int( nif->get<quint32>(idx.parent().parent(), "Weights Per Vertex") );
				if ( weightsPerVertex > 1 )
					s /= weightsPerVertex;
			}
		}
		Shape::drawVerts( GLView::Settings::vertexPointSize, s );
	} else if ( n == "Normals" || n == "Tangents" ) {
		int	btnMask = ( n == "Normals" ? 0x04 : 0x03 );
		int	s = -1;
		if ( n == p )
			s = idx.row();
		Shape::drawVerts( GLView::Settings::tbnPointSize, s );
		float	normalScale = std::max< float >( boundSphere.radius / 16.0f, 2.5f / 512.0f ) * viewTrans().scale;
		Shape::drawNormals( btnMask, s, normalScale );
	} else if ( n == "Skin" ) {
		auto	iSkin = nif->getBlockIndex( nif->getLink( idx.parent(), "Skin" ) );
		if ( iSkin.isValid() && nif->isNiBlock( iSkin, "BSSkin::Instance" ) ) {
			auto	iBoneData = nif->getBlockIndex( nif->getLink( iSkin, "Data" ) );
			if ( iBoneData.isValid() && nif->isNiBlock( iBoneData, "BSSkin::BoneData" ) ) {
				auto	iBones = nif->getIndex( iBoneData, "Bone List" );
				int	numBones;
				if ( iBones.isValid() && nif->isArray( iBones ) && ( numBones = nif->rowCount( iBones ) ) > 0 ) {
					for ( int i = 0; i < numBones; i++ ) {
						auto	iBone = nif->getIndex( iBones, i );
						if ( !iBone.isValid() )
							continue;
						BoundSphere	sph( nif, iBone );
						if ( !( sph.radius > 0.0f ) )
							continue;
						Transform	t( nif, iBone );
						sph.center -= t.translation;
						t.rotation = t.rotation.inverted();
						t.translation = Vector3( 0.0f, 0.0f, 0.0f );
						t.scale = 1.0f / t.scale;
						sph.radius *= t.scale;
						sph.center = t * sph.center;
						drawBoundingSphere( sph, FloatVector4( 1.0f, 1.0f, 1.0f, 0.33f ) );
						bindShape();
					}
				}
			}
		}
	} else {
		int	s = -1;
		if ( n == p )
			s = idx.row();

		QModelIndex	iMeshlets;
		if ( s < 0 && n == "Meshlets" && ( iMeshlets = nif->getIndex( idx.parent(), "Meshlets" ) ).isValid() ) {
			// draw all meshlets
			qsizetype	triangleOffset = 0;
			qsizetype	triangleCount = 0;
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			int	numMeshlets = nif->rowCount( iMeshlets );
			for ( int i = 0; i < numMeshlets && triangleOffset < numTriangles; i++ ) {
				triangleCount = nif->get<quint32>( nif->getIndex( iMeshlets, i ), "Triangle Count" );
				triangleCount = std::min< qsizetype >( triangleCount, numTriangles - triangleOffset );
				if ( triangleCount < 1 )
					continue;

				// generate meshlet color from index
				std::uint32_t	j = std::uint32_t( i );
				j = ( j & 0x1249U ) | ( ( j & 0x2492U ) << 7 ) | ( ( j & 0x4924U ) << 14 );
				j = ( ( j & 0x00010101U ) << 7 ) | ( ( j & 0x00080808U ) << 3 )
					| ( ( j & 0x00404040U ) >> 1 ) | ( ( j & 0x02020200U ) >> 5 ) | ( ( j & 0x10101000U ) >> 9 );
				j = ~j;
				Shape::drawTriangles( triangleOffset, triangleCount, FloatVector4( j ) / 255.0f );
				triangleOffset += triangleCount;
			}
			Shape::drawWireframe( FloatVector4( 0.125f ).blendValues( scene->wireframeColor, 0x07 ) );
			glDepthFunc( GL_LEQUAL );
		} else {
			// General wireframe
			Shape::drawWireframe( scene->wireframeColor );
		}

		if ( s >= 0 && ( n == "Triangles" || n == "Meshlets" ) ) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glDepthFunc( GL_ALWAYS );

			if ( n == "Triangles" ) {
				// draw selected triangle
				Shape::drawTriangles( s, 1, scene->highlightColor );
			} else if ( ( iMeshlets = nif->getIndex( idx.parent().parent(), "Meshlets" ) ).isValid() ) {
				// draw selected meshlet
				qsizetype	triangleOffset = 0;
				qsizetype	triangleCount = 0;
				for ( int i = 0; i <= s; i++ ) {
					triangleOffset += triangleCount;
					triangleCount = nif->get<quint32>( nif->getIndex( iMeshlets, i ), "Triangle Count" );
				}
				Shape::drawTriangles( triangleOffset, triangleCount, scene->highlightColor );
			}
		}
	}

	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_POLYGON_OFFSET_LINE );
	glDisable( GL_POLYGON_OFFSET_POINT );

#if 0 && !defined(QT_NO_DEBUG)
	drawSphereSimple(boundSphere.center, boundSphere.radius, 72);
#endif
}

BoundSphere BSMesh::bounds() const
{
	if ( needUpdateBounds ) {
		needUpdateBounds = false;
		if ( verts.size() ) {
			boundSphere = BoundSphere( verts );
		} else {
			boundSphere = dataBound;
		}
	}

	return worldTrans() * boundSphere;
}

QString BSMesh::textStats() const
{
	return QString();
}

int BSMesh::meshCount()
{
	return meshes.size();
}

void BSMesh::drawVerts() const
{
	int	vertexSelected = -1;

	if ( !scene->selecting && scene->currentBlock == iBlock ) {
		for ( const auto & idx = scene->currentIndex; idx.isValid(); ) {
			// Name of this index
			auto n = idx.data( NifSkopeDisplayRole ).toString();
			if ( !( n == "Vertices" || n == "UVs" || n == "UVs 2" || n == "Vertex Colors"
					|| n == "Normals" || n == "Tangents" || n == "Weights" ) ) {
				break;
			}
			// Name of this index's parent
			auto p = idx.parent().data( NifSkopeDisplayRole ).toString();
			if ( n == p ) {
				vertexSelected = idx.row();
				if ( n == "Weights" ) {
					auto	nif = NifModel::fromValidIndex( idx );
					int	weightsPerVertex = 0;
					if ( nif )
						weightsPerVertex = nif->get<int>( idx.parent().parent(), "Weights Per Vertex" );
					if ( weightsPerVertex > 0 )
						vertexSelected /= weightsPerVertex;
					else
						vertexSelected = -1;
				}
			}
			break;
		}
	}

	Shape::drawVerts( GLView::Settings::vertexSelectPointSize, vertexSelected );
}

QModelIndex BSMesh::vertexAt( int c ) const
{
	if ( !( c >= 0 && c < verts.size() ) )
		return QModelIndex();

	QModelIndex	iMeshData = iBlock;
	if ( !iMeshData.isValid() )
		return QModelIndex();
	for ( auto nif = NifModel::fromValidIndex( iMeshData ); nif && nif->blockInherits( iMeshData, "BSGeometry" ); ) {
		iMeshData = nif->getIndex( iMeshData, "Meshes" );
		if ( !( iMeshData.isValid() && nif->isArray( iMeshData ) ) )
			break;
		int	l = 0;
		if ( gpuLODs.isEmpty() )
			l = int( lodLevel );
		iMeshData = nif->getIndex( iMeshData, l );
		if ( !iMeshData.isValid() )
			break;
		iMeshData = nif->getIndex( iMeshData, "Mesh" );
		if ( !iMeshData.isValid() )
			break;
		iMeshData = nif->getIndex( iMeshData, "Mesh Data" );
		if ( !iMeshData.isValid() )
			break;
		QModelIndex	iVerts;
		const auto &	idx = scene->currentIndex;
		if ( idx.isValid() ) {
			auto	n = idx.data( NifSkopeDisplayRole ).toString();
			if ( n == "UVs" )
				iVerts = nif->getIndex( iMeshData, "UVs" );
			else if ( n == "UVs 2" )
				iVerts = nif->getIndex( iMeshData, "UVs 2" );
			else if ( n == "Vertex Colors" )
				iVerts = nif->getIndex( iMeshData, "Vertex Colors" );
			else if ( n == "Normals" )
				iVerts = nif->getIndex( iMeshData, "Normals" );
			else if ( n == "Tangents" )
				iVerts = nif->getIndex( iMeshData, "Tangents" );
			else if ( n == "Weights" )
				iVerts = nif->getIndex( iMeshData, "Weights" );
		}
		if ( !iVerts.isValid() )
			iVerts = nif->getIndex( iMeshData, "Vertices" );
		if ( !( iVerts.isValid() && nif->isArray( iVerts ) ) )
			break;
		int	n = nif->rowCount( iVerts );
		if ( n <= 0 )
			break;
		return nif->getIndex( iVerts, int( std::int64_t( c ) * n / verts.size() ) );
	}
	return QModelIndex();
}

void BSMesh::updateImpl(const NifModel* nif, const QModelIndex& index)
{
	qDebug() << "updateImpl";
	Shape::updateImpl(nif, index);
	if ( index != iBlock )
		return;

	iData = index;
	iMeshes = nif->getIndex(index, "Meshes");
	meshes.clear();
	for ( int i = 0; i < 4; i++ ) {
		auto meshArray = nif->getIndex( iMeshes, i );
		bool hasMesh = nif->get<bool>( nif->getIndex( meshArray, 0 ) );
		if ( hasMesh ) {
			auto mesh = std::make_shared<MeshFile>( nif, nif->getIndex( meshArray, 1 ) );
			if ( mesh->isValid() ) {
				meshes.append(mesh);
				if ( i > 0 || mesh->lods.size() > 0 )
					emit nif->lodSliderChanged(true);
			}
		}
	}
}

void BSMesh::updateData(const NifModel* nif)
{
	qDebug() << "updateData";
	clearHash();
	resetSkinning();
	resetVertexData();
	resetSkeletonData();
	skinID = -1;
	numWeights = 0;
	gpuLODs.clear();
	boneNames.clear();

	if ( meshes.size() == 0 )
		return;

	bool hasMeshLODs = meshes[0]->lods.size() > 0;
	int lodCount = (hasMeshLODs) ? meshes[0]->lods.size() + 1 : meshes.size();

	if ( hasMeshLODs && meshes.size() > 1 ) {
		qWarning() << "Both static and skeletal mesh LODs exist";
	}

	lodLevel = std::min(scene->lodLevel, Scene::LodLevel(lodCount - 1));

	const BoneWeightsUNorm *	weights = nullptr;
	auto meshIndex = (hasMeshLODs) ? 0 : lodLevel;
	if ( lodCount > int(lodLevel) ) {
		auto& mesh = meshes[meshIndex];
		if ( lodLevel > 0 && int(lodLevel) <= mesh->lods.size() ) {
			triangles = mesh->lods[lodLevel - 1];
		}
		else {
			triangles = mesh->triangles;
		}
		lodTriangleCount = triangles.size();
		verts = mesh->positions;
		removeInvalidIndices();
		coords.resize( mesh->coords2.isEmpty() ? 1 : 2 );
		coords.first() = mesh->coords1;
		if ( !mesh->coords2.isEmpty() )
			coords[1] = mesh->coords2;
		colors = mesh->colors;
		hasVertexColors = !colors.empty();
		norms = mesh->normals;
		bitangents = mesh->tangents;
		mesh->calculateBitangents( tangents );
		boneWeights0.clear();
		boneWeights1.clear();
		numWeights = int( mesh->weights.size() );
		if ( numWeights >= verts.size() )
			weights = mesh->weights.constData();
		gpuLODs = mesh->lods;

		boundSphere = BoundSphere( verts );
		boundSphere.applyInv( viewTrans() );
	}

	auto links = nif->getChildLinks(nif->getBlockNumber(iBlock));
	for ( const auto link : links ) {
		auto idx = nif->getBlockIndex(link);
		if ( nif->blockInherits(idx, "BSSkin::Instance") ) {
			iSkin = idx;
			iSkinData = nif->getBlockIndex(nif->getLink(nif->getIndex(idx, "Data")));
			skinID = nif->getBlockNumber(iSkin);

			qsizetype numBones = nif->get<int>(iSkinData, "Num Bones");
			boneData.fill( BoneData(), numBones );

			auto iBones = nif->getLinkArray(iSkin, "Bones");
			qsizetype validBones = 0;
			for ( qsizetype i = 0; i < iBones.size(); i++ ) {
				auto b = iBones.at( i );
				if ( i < numBones )
					boneData[i].bone = b;
				if ( b == -1 )
					continue;
				auto iBone = nif->getBlockIndex(b);
				boneNames.append(nif->resolveString(iBone, "Name"));
				validBones++;
			}
			isSkinned = ( validBones >= numBones );

			auto iBoneList = nif->getIndex(iSkinData, "Bone List");
			for ( int i = 0; i < numBones; i++ )
				boneData[i].setTransform( nif, nif->getIndex( iBoneList, i ) );

			if ( weights && isSkinned ) {
				size_t	numVerts = size_t( verts.size() );
				boneWeights0.assign( numVerts, FloatVector4( 0.0f ) );
				for ( size_t i = 0; i < numVerts; i++ ) {
					size_t	k = 0;
					for ( const auto & bw : weights[i].weightsUNORM ) {
						unsigned int	b = bw.bone;
						float	w = bw.weight;
						if ( b < (unsigned int) numBones && b < 256U && w > ( !b ? 0.00005f : 0.00001f ) ) {
							w = float( int(b) ) + ( std::min( w, 1.0f ) * float( 65535.0 / 65536.0 ) );
							if ( k < 4 ) {
								boneWeights0[i][k] = w;
							} else if ( k < 8 ) {
								if ( boneWeights1.size() < numVerts ) [[unlikely]]
									boneWeights1.assign( numVerts, FloatVector4( 0.0f ) );
								boneWeights1[i][k & 3] = w;
							}
							k++;
						}
					}
				}
			}
		}
	}
	// Do after dependent blocks above
	for ( const auto link : links ) {
		auto idx = nif->getBlockIndex(link);
		if ( nif->blockInherits(idx, "SkinAttach") ) {
			boneNames = nif->getArray<QString>(idx, "Bones");
			if ( std::all_of(boneNames.begin(), boneNames.end(), [](const QString& name) { return name.isEmpty(); }) ) {
				boneNames.clear();
				auto iBones = nif->getLinkArray(nif->getIndex(iSkin, "Bones"));
				for ( const auto& b : iBones ) {
					auto iBone = nif->getBlockIndex(b);
					boneNames.append(nif->resolveString(iBone, "Name"));
				}
			}
		}
	}
}
