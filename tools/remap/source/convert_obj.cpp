/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "remap.h"



/*
   ConvertSurface()
   converts a bsp drawsurface to an obj chunk
 */

static int firstLightmap = 0;
static int lastLightmap = -1;

static int objVertexCount = 0;
static int objLastShaderNum = -1;

static void ConvertSurfaceToOBJ( FILE *f, int modelNum, int surfaceNum, const Vector3& origin, const std::vector<int>& lmIndices ){

}



/*
   ConvertModel()
   exports a bsp model to an ase chunk
 */

static void ConvertModelToOBJ( FILE *f, int modelNum, const Vector3& origin, const std::vector<int>& lmIndices ){

}



/*
   ConvertShader()
   exports a bsp shader to an ase chunk
 */

static void ConvertShaderToMTL( FILE *f, const bspShader_t& shader ){
	shaderInfo_t    *si;
	char filename[ 1024 ];


	/* get shader */
	si = ShaderInfoForShader( shader.shader );
	if ( si == NULL ) {
		Sys_Warning( "NULL shader in BSP\n" );
		return;
	}

	/* set bitmap filename */
	if ( si->shaderImage->filename.c_str()[ 0 ] != '*' ) {
		strcpy( filename, si->shaderImage->filename.c_str() );
	}
	else{
		sprintf( filename, "%s.tga", si->shader.c_str() );
	}

	/* blender hates this, so let's not do it
	for( c = filename; *c; c++ )
		if( *c == '/' )
			*c = '\\';
	*/

	/* print shader info */
	fprintf( f, "newmtl %s\r\n", shader.shader );
	fprintf( f, "Kd %f %f %f\r\n", si->color[ 0 ], si->color[ 1 ], si->color[ 2 ] );
	if ( shadersAsBitmap ) {
		fprintf( f, "map_Kd %s\r\n", shader.shader );
	}
	else{
		/* blender hates this, so let's not do it
		    fprintf( f, "map_Kd ..\\%s\r\n", filename );
		 */
		fprintf( f, "map_Kd ../%s\r\n", filename );
	}
}

static void ConvertLightmapToMTL( FILE *f, const char *base, int lightmapNum ){
	/* print shader info */
	fprintf( f, "newmtl lm_%04d\r\n", lightmapNum );
	if ( lightmapNum >= 0 ) {
		/* blender hates this, so let's not do it
		    fprintf( f, "map_Kd %s\\" EXTERNAL_LIGHTMAP "\r\n", base, lightmapNum );
		 */
		if( shadersAsBitmap )
			fprintf( f, "map_Kd maps/%s/" EXTERNAL_LIGHTMAP "\r\n", base, lightmapNum );
		else
			fprintf( f, "map_Kd %s/" EXTERNAL_LIGHTMAP "\r\n", base, lightmapNum );
	}
}


int Convert_CountLightmaps( const char* dirname ){
	int lightmapCount;
	//FIXME numBSPLightmaps is 0, must be bspLightBytes / ( g_game->lightmapSize * g_game->lightmapSize * 3 )
	for ( lightmapCount = 0; lightmapCount < numBSPLightmaps; lightmapCount++ )
		;
	for ( ; ; lightmapCount++ )
	{
		char buf[1024];
		snprintf( buf, sizeof( buf ), "%s/" EXTERNAL_LIGHTMAP, dirname, lightmapCount );
		buf[sizeof( buf ) - 1] = 0;
		if ( !FileExists( buf ) ) {
			break;
		}
	}
	return lightmapCount;
}

/* manage external lms, possibly referenced by q3map2_%mapname%.shader */
void Convert_ReferenceLightmaps( const char* base, std::vector<int>& lmIndices ){
	char shaderfile[256];
	sprintf( shaderfile, "%s/q3map2_%s.shader", g_game->shaderPath, base );
	LoadScriptFile( shaderfile );
	/* tokenize it */
	while ( GetToken( true ) ) /* test for end of file */
	{
		char shadername[256];
		strcpy( shadername, token );

		/* handle { } section */
		if ( !( GetToken( true ) && strEqual( token, "{" ) ) )
			Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s\nFile location be: %s",
			       shaderfile, scriptline, token, g_strLoadedFileLocation );
		while ( GetToken( true ) && !strEqual( token, "}" ) )
		{
			/* parse stage directives */
			if ( strEqual( token, "{" ) ) {
				while ( GetToken( true ) && !strEqual( token, "}" ) )
				{
					if ( strEqual( token, "{" ) )
						Sys_FPrintf( SYS_WRN, "WARNING9: %s : line %d : opening brace inside shader stage\n", shaderfile, scriptline );

					/* digest any images */
					if ( striEqual( token, "map" ) ) {
						/* get an image */
						GetToken( false );
						if ( *token != '*' && *token != '$' ) {
							// map maps/bake_test_1/lm_0004.tga
							int lmindex;
							int okcount = 0;
							if( sscanf( token + strlen( token ) - ( strlen( EXTERNAL_LIGHTMAP ) + 1 ), "/" EXTERNAL_LIGHTMAP "%n", &lmindex, &okcount )
							    && okcount == ( strlen( EXTERNAL_LIGHTMAP ) + 1 ) ){
								for ( size_t i = 0; i < bspShaders.size(); ++i ){ // find bspShaders[i]<->lmindex pair
									if( strEqual( bspShaders[i].shader, shadername ) ){
										lmIndices[i] = lmindex;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}




/*
   ConvertBSPToASE()
   exports an 3d studio ase file from the bsp
 */

int ConvertBSPToOBJ( char *bspName ){
	int modelNum;
	FILE            *f, *fmtl;
	entity_t        *e;
	const char      *key;
	std::vector<int> lmIndices( bspShaders.size(), -1 );


	/* note it */
	Sys_Printf( "--- Convert BSP to OBJ ---\n" );

	/* create the ase filename from the bsp name */
	const auto dirname = StringStream( PathExtensionless( bspName ) );
	const auto name = StringStream( dirname, ".obj" );
	Sys_Printf( "writing %s\n", name.c_str() );
	const auto mtlname = StringStream( dirname, ".mtl" );
	Sys_Printf( "writing %s\n", mtlname.c_str() );
	const auto base = StringStream<64>( PathFilename( bspName ) );

	/* open it */
	f = SafeOpenWrite( name );
	fmtl = SafeOpenWrite( mtlname );

	/* print header */
	fprintf( f, "o %s\r\n", base.c_str() );
	fprintf( f, "# Generated by Q3Map2 (ydnar) -convert -format obj\r\n" );
	fprintf( f, "mtllib %s.mtl\r\n", base.c_str() );

	fprintf( fmtl, "# Generated by Q3Map2 (ydnar) -convert -format obj\r\n" );
	if ( lightmapsAsTexcoord ) {
		lastLightmap = Convert_CountLightmaps( dirname ) - 1;
		Convert_ReferenceLightmaps( base, lmIndices );
	}
	else
	{
		for ( const bspShader_t& shader : bspShaders )
		{
			ConvertShaderToMTL( fmtl, shader );
		}
	}

	/* walk entity list */
	for ( std::size_t i = 0; i < entities.size(); ++i )
	{
		/* get entity and model */
		e = &entities[ i ];
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			key = e->valueForKey( "model" );
			if ( key[ 0 ] != '*' ) {
				continue;
			}
			modelNum = atoi( key + 1 );
		}

		/* convert model */
		ConvertModelToOBJ( f, modelNum, e->vectorForKey( "origin" ), lmIndices );
	}

	if ( lightmapsAsTexcoord ) {
		for ( int i = firstLightmap; i <= lastLightmap; i++ )
			ConvertLightmapToMTL( fmtl, base, i );
	}

	/* close the file and return */
	fclose( f );
	fclose( fmtl );

	/* return to sender */
	return 0;
}
