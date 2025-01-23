#include "about_dialog.h"

AboutDialog::AboutDialog( QWidget * parent )
	: QDialog( parent )
{
	ui.setupUi( this );

	setAttribute( Qt::WA_DeleteOnClose );

#ifdef NIFSKOPE_REVISION
	this->setWindowTitle( tr( "About NifSkope %1 (revision %2)" ).arg( NIFSKOPE_VERSION, NIFSKOPE_REVISION ) );
#else
	this->setWindowTitle( tr( "About NifSkope %1" ).arg( NIFSKOPE_VERSION ) );
#endif
	QString text = tr( R"rhtml(
	<p>NifSkope is a tool for opening and editing the NetImmerse file format (NIF).</p>

	<p>NifSkope is free software available under a BSD license.
	The source is available via <a href='https://github.com/niftools/nifskope'>GitHub</a></p>

	<p>The most recent version of NifSkope can be downloaded from the <a href='https://github.com/niftools/nifskope/releases'>
	official GitHub release page</a>.</p>

	<p>A detailed changelog and the latest developmental builds of NifSkope
	<a href='https://github.com/fo76utils/nifskope/releases'>can be found here</a>.</p>

	<p>For the generation of mopp code on Windows builds, NifSkope uses <a href='http://www.havok.com'>Havok(R)</a>:<br>
	Copyright © 1999-2008 Havok.com Inc. (and its Licensors).<br>
	All Rights Reserved.</p>

	<p>NifSkope uses <a href='http://gli.g-truc.net/'>OpenGL Image (GLI)</a>:<br>
	MIT License<br>
	Copyright © 2010 - 2016 G-Truc Creation</p>

	<p>For the generation of convex hulls, NifSkope uses <a href='http://www.qhull.org'>Qhull</a>:<br>
	Copyright © 1993-2015  C.B. Barber and The Geometry Center.<br><center>
						Qhull, Copyright © 1993-2015<br><br>

								C.B. Barber<br>
							   Arlington, MA <br><br>

								   and<br><br>

		   The National Science and Technology Research Center for<br>
			Computation and Visualization of Geometric Structures<br>
							(The Geometry Center)<br>
						   University of Minnesota<br><br>

						   email: qhull@qhull.org<br><br>

	This software includes Qhull from C.B. Barber and The Geometry Center.<br>
	Qhull is copyrighted as noted above.  Qhull is free software and may <br>
	be obtained via http from www.qhull.org.  It may be freely copied, modified, <br>
	and redistributed under the following conditions:<br><br>

	1. All copyright notices must remain intact in all files.<br><br>

	2. A copy of this text file must be distributed along with any copies <br>
	   of Qhull that you redistribute; this includes copies that you have <br>
	   modified, or copies of programs or other software products that <br>
	   include Qhull.<br><br>

	3. If you modify Qhull, you must include a notice giving the<br>
	   name of the person performing the modification, the date of<br>
	   modification, and the reason for such modification.<br><br>

	4. When distributing modified versions of Qhull, or other software <br>
	   products that include Qhull, you must provide notice that the original <br>
	   source code may be obtained as noted above.<br><br>

	5. There is no warranty or other guarantee of fitness for Qhull, it is <br>
	   provided solely "as is".  Bug reports or fixes may be sent to <br>
	   qhull_bug@qhull.org; the authors may or may not act on them as <br>
	   they desire.<br></center>
	</p>

	<p>For bounding sphere calculation, NifSkope uses <a href='https://github.com/hbf/miniball'>Miniball</a> by Kaspar Fischer, Bernd Gärtner and Martin Kutz, the code is available under the <a href='http://www.apache.org/licenses/LICENSE-2.0.html'>Apache 2 License</a>.
	</p>

	<p>Starfield meshlet and LOD generation are based on code from <a href='https://github.com/zeux/meshoptimizer'>meshoptimizer</a> (MIT License, Copyright © 2016-2025 Arseny Kapoulkine) and <a href='https://github.com/microsoft/DirectXMesh'>DirectXMesh</a> (MIT License, Copyright © by Microsoft Corporation).
	</p>

	<p><a href='https://github.com/syoyo/tinygltf'>Tiny glTF</a> is copyright © 2015-present by Syoyo Fujita, Aurélien Chatelain and many contributors under the MIT License.
	</p>

	<p><a href='https://github.com/nlohmann/json'>JSON for Modern C++</a> library is copyright © 2013-2025 by Niels Lohmann, MIT License.
	</p>

	<p><a href='https://github.com/Cyan4973/xxHash'>xxHash</a> is copyright © 2012-2023 by Yann Collet, BSD 2-Clause License.
	</p>
	)rhtml" );

	ui.text->setText( text );
}
