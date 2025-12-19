//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the rendering 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "rendermanager.h"
#include "legion.h"
#include "uimanager.h"
#include "worldmanager.h"
#include "materialsystem/imaterialsystem.h"
#include "tier2/tier2.h"
#include "tier2/meshutils.h"


//-----------------------------------------------------------------------------
// Camera property
//-----------------------------------------------------------------------------
DEFINE_FIXEDSIZE_ALLOCATOR( CCameraProperty, 1, CMemoryPool::GROW_SLOW );


CCameraProperty::CCameraProperty()
{
	m_Origin.Init();
	m_Angles.Init();
	m_Velocity.Init();
	m_AngVelocity.Init();
}

void CCameraProperty::GetForward( Vector *pForward )
{
	AngleVectors( m_Angles, pForward );
}


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CRenderManager s_RenderManager;
extern CRenderManager *g_pRenderManager = &s_RenderManager;

	
//-----------------------------------------------------------------------------
// Game initialization
//-----------------------------------------------------------------------------
bool CRenderManager::Init()
{
	m_bRenderWorldFullscreen = true;
	return true;
}

void CRenderManager::Shutdown()
{
}

	
//-----------------------------------------------------------------------------
// Level initialization
//-----------------------------------------------------------------------------
LevelRetVal_t CRenderManager::LevelInit( bool bFirstCall )
{
	return FINISHED;
}

LevelRetVal_t CRenderManager::LevelShutdown( bool bFirstCall )
{
	return FINISHED;
}

	
//-----------------------------------------------------------------------------
// Property allocation
//-----------------------------------------------------------------------------
CCameraProperty *CRenderManager::CreateCameraProperty()
{
	return new CCameraProperty;
}

void CRenderManager::DestroyCameraProperty( CCameraProperty *pProperty )
{
	delete pProperty;
}


//-----------------------------------------------------------------------------
// Sets the rectangle to draw into
//-----------------------------------------------------------------------------
void CRenderManager::RenderWorldFullscreen()
{
	m_bRenderWorldFullscreen = true;
}

void CRenderManager::RenderWorldInRect( int x, int y, int nWidth, int nHeight )
{
	m_bRenderWorldFullscreen = false;
	m_nRenderX = x;
	m_nRenderY = y;
	m_nRenderWidth = nWidth;
	m_nRenderHeight = nHeight;
}

	
//-----------------------------------------------------------------------------
// Done completely client-side, want total smoothness, so simulate at render interval
//-----------------------------------------------------------------------------
void CRenderManager::UpdateLocalPlayerCamera()
{
	float dt = IGameManager::DeltaTime();
	CCameraProperty *pCamera = g_pWorldManager->GetLocalPlayer()->m_pCameraProperty;
	VectorMA( pCamera->m_Origin, dt, pCamera->m_Velocity, pCamera->m_Origin );
	VectorMA( pCamera->m_Angles, dt, pCamera->m_AngVelocity, pCamera->m_Angles );
}


//-----------------------------------------------------------------------------
// Per-frame update
//-----------------------------------------------------------------------------
void CRenderManager::Update( )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	if ( GetLevelState() == NOT_IN_LEVEL )
	{
		g_pMaterialSystem->BeginFrame( 0 );
		pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
		pRenderContext->ClearBuffers( true, true );
		g_pUIManager->DrawUI();
		g_pMaterialSystem->EndFrame();
		g_pMaterialSystem->SwapBuffers();
		return;
	}

	UpdateLocalPlayerCamera();

	g_pMaterialSystem->BeginFrame( 0 );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 ); 
	pRenderContext->ClearBuffers( true, true );
	RenderWorld();
	g_pWorldManager->DrawESP();
	g_pUIManager->DrawUI();
	g_pMaterialSystem->EndFrame();
	g_pMaterialSystem->SwapBuffers();
}


//-----------------------------------------------------------------------------
// Sets up the camera
//-----------------------------------------------------------------------------
void CRenderManager::SetupCameraRenderState( )
{
	CCameraProperty *pCamera = g_pWorldManager->GetLocalPlayer()->m_pCameraProperty;

	matrix3x4_t cameraToWorld;
	AngleMatrix( pCamera->m_Angles, pCamera->m_Origin, cameraToWorld );

	matrix3x4_t matRotate;
	matrix3x4_t matRotateZ;
	MatrixBuildRotationAboutAxis( Vector(0,0,1), -90, matRotateZ );
	MatrixMultiply( cameraToWorld, matRotateZ, matRotate );

	matrix3x4_t matRotateX;
	MatrixBuildRotationAboutAxis( Vector(1,0,0), 90, matRotateX );
	MatrixMultiply( matRotate, matRotateX, matRotate );

	matrix3x4_t view;
	MatrixInvert( matRotate, view );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadMatrix( view );
}

	
//-----------------------------------------------------------------------------
// Set up a projection matrix for a 90 degree fov
//-----------------------------------------------------------------------------

// FIXME: Better control over Z range
#define ZNEAR 0.1f
#define ZFAR 10000.0f

void CRenderManager::SetupProjectionMatrix( int nWidth, int nHeight, float flFOV )
{
	VMatrix proj;
	float flZNear = ZNEAR;
	float flZFar = ZFAR;
	float flApsectRatio = (nHeight != 0.0f) ? (float)nWidth / (float)nHeight : 100.0f;

	float halfWidth = tan( flFOV * M_PI / 360.0 );
	float halfHeight = halfWidth / flApsectRatio;

	memset( proj.Base(), 0, sizeof( proj ) );
	proj[0][0]  = 1.0f / halfWidth;
	proj[1][1]  = 1.0f / halfHeight;
	proj[2][2] = flZFar / ( flZNear - flZFar );
	proj[3][2] = -1.0f;
	proj[2][3] = flZNear * flZFar / ( flZNear - flZFar );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadMatrix( proj );
}


//-----------------------------------------------------------------------------
// Set up a orthographic projection matrix
//-----------------------------------------------------------------------------
void CRenderManager::SetupOrthoMatrix( int nWidth, int nHeight )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->Ortho( 0, 0, nWidth, nHeight, -1.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// Renders the world
//-----------------------------------------------------------------------------
void CRenderManager::RenderWorld()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	if ( m_bRenderWorldFullscreen )
	{
		m_nRenderX = m_nRenderY = 0;
		pRenderContext->GetRenderTargetDimensions( m_nRenderWidth, m_nRenderHeight );
	}

	pRenderContext->DepthRange( 0, 1 );
	pRenderContext->Viewport( m_nRenderX, m_nRenderY, m_nRenderWidth, m_nRenderHeight );

	SetupProjectionMatrix( m_nRenderWidth, m_nRenderHeight, 90 );

	SetupCameraRenderState();

	g_pWorldManager->DrawWorld();

	DrawESP();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}


//-----------------------------------------------------------------------------
// Draws ESP
//-----------------------------------------------------------------------------
void CRenderManager::DrawESP()
{
	if ( !cl_esp.GetBool() )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Switch to 2D for drawing
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	SetupOrthoMatrix( m_nRenderWidth, m_nRenderHeight );

	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( "vgui/white", TEXTURE_GROUP_VGUI );
	pRenderContext->Bind( pMaterial );

	// Get entities
	for ( int i = 0; i < g_pWorldManager->m_Entities.Count(); i++ )
	{
		CEntity &ent = g_pWorldManager->m_Entities[i];
		if ( !ent.m_bEnemy )
			continue;

		Vector screenPos;
		if ( WorldToScreen( ent.m_vecOrigin, screenPos ) )
		{
			int x = screenPos.x;
			int y = screenPos.y;
			int boxSize = 20;

			// Draw box
			if ( cl_esp_box.GetBool() )
			{
				IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );
				CMeshBuilder meshBuilder;
				meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

				meshBuilder.Position3f( x - boxSize, y - boxSize, 0 );
				meshBuilder.Color4ub( 255, 0, 0, 255 );
				meshBuilder.TexCoord2f( 0, 0, 0 );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3f( x + boxSize, y - boxSize, 0 );
				meshBuilder.Color4ub( 255, 0, 0, 255 );
				meshBuilder.TexCoord2f( 0, 1, 0 );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3f( x + boxSize, y + boxSize, 0 );
				meshBuilder.Color4ub( 255, 0, 0, 255 );
				meshBuilder.TexCoord2f( 0, 1, 1 );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3f( x - boxSize, y + boxSize, 0 );
				meshBuilder.Color4ub( 255, 0, 0, 255 );
				meshBuilder.TexCoord2f( 0, 0, 1 );
				meshBuilder.AdvanceVertex();

				meshBuilder.End();
				pMesh->Draw();
			}

			// Draw line from center to entity
			if ( cl_esp_line.GetBool() )
			{
				IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );
				CMeshBuilder meshBuilder;
				meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

				meshBuilder.Position3f( m_nRenderWidth / 2, m_nRenderHeight / 2, 0 );
				meshBuilder.Color4ub( 0, 255, 0, 255 );
				meshBuilder.TexCoord2f( 0, 0, 0 );
				meshBuilder.AdvanceVertex();

				meshBuilder.Position3f( x, y, 0 );
				meshBuilder.Color4ub( 0, 255, 0, 255 );
				meshBuilder.TexCoord2f( 0, 1, 0 );
				meshBuilder.AdvanceVertex();

				meshBuilder.End();
				pMesh->Draw();
			}

			// Draw name - skip for simplicity
		}
	}

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}


//-----------------------------------------------------------------------------
// World to screen conversion
//-----------------------------------------------------------------------------
bool CRenderManager::WorldToScreen( const Vector &worldPos, Vector &screenPos )
{
	CCameraProperty *pCamera = g_pWorldManager->GetLocalPlayer()->m_pCameraProperty;
	Vector vecToPos = worldPos - pCamera->m_Origin;
	float flDist = vecToPos.Length();
	if ( flDist < 1.0f )
		return false;

	Vector forward, right, up;
	AngleVectors( pCamera->m_Angles, &forward, &right, &up );

	float dot = DotProduct( vecToPos, forward );
	if ( dot < 0 )
		return false; // Behind camera

	float flFOV = 90.0f * M_PI / 180.0f;
	float flTanHalfFOV = tan( flFOV / 2.0f );

	float flScreenX = DotProduct( vecToPos, right ) / ( flDist * flTanHalfFOV );
	float flScreenY = DotProduct( vecToPos, up ) / ( flDist * flTanHalfFOV );

	screenPos.x = (flScreenX * 0.5f + 0.5f) * m_nRenderWidth;
	screenPos.y = (0.5f - flScreenY * 0.5f) * m_nRenderHeight;
	screenPos.z = 0;

	return true;
}