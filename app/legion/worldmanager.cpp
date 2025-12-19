//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains all world state--the main game database
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "worldmanager.h"
#include "legion.h"
#include "heightfield.h"
#include "rendermanager.h"

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CWorldManager s_WorldManager;
extern CWorldManager *g_pWorldManager = &s_WorldManager;


//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
ConVar cam_forwardspeed( "cam_forwardspeed", "100", FCVAR_CHEAT, "Sets the camera forward speed" );
ConVar cam_backwardspeed( "cam_backwardspeed", "100", FCVAR_CHEAT, "Sets the camera backward speed" );
ConVar cl_esp( "cl_esp", "0", FCVAR_CHEAT, "Enable ESP" );
ConVar cl_esp_box( "cl_esp_box", "1", FCVAR_CHEAT, "Draw ESP boxes" );
ConVar cl_esp_line( "cl_esp_line", "1", FCVAR_CHEAT, "Draw ESP lines" );
ConVar cl_esp_name( "cl_esp_name", "1", FCVAR_CHEAT, "Draw ESP names" );
ConVar cl_thirdperson( "cl_thirdperson", "0", FCVAR_CHEAT, "Enable third person view" );
ConVar cl_spinbot( "cl_spinbot", "0", FCVAR_CHEAT, "Enable spinbot" );
ConVar cl_spinbot_speed( "cl_spinbot_speed", "100", FCVAR_CHEAT, "Spinbot speed" );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CWorldManager::CWorldManager()
{
	m_pHeightField = NULL;
}

CWorldManager::~CWorldManager()
{
	Assert( m_pHeightField == NULL );
}


//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
LevelRetVal_t CWorldManager::LevelInit( bool bFirstCall )
{
	if ( !bFirstCall )
		return FINISHED;

	Assert( !m_pHeightField );
	m_pHeightField = new CHeightField( 6, 6, 4 );
	if ( !m_pHeightField->LoadHeightFromFile( "maps/testheight.psd" ) )
		return FAILED;

	CreateEntities();
	SetInitialLocalPlayerPosition();
	return FINISHED;
}

LevelRetVal_t CWorldManager::LevelShutdown( bool bFirstCall )
{
	if ( !bFirstCall )
		return FINISHED;

	DestroyEntities();

	if ( m_pHeightField )
	{
		delete m_pHeightField;
		m_pHeightField = NULL;
	}
	return FINISHED;
}


//-----------------------------------------------------------------------------
// Create/ destroy entities
//-----------------------------------------------------------------------------
void CWorldManager::CreateEntities()
{
	m_PlayerEntity.m_pCameraProperty = g_pRenderManager->CreateCameraProperty();
	m_PlayerEntity.m_vecPosition.Init();
	m_PlayerEntity.m_angAngles.Init();
	m_PlayerEntity.m_angCameraAngles.Init();

	// Create some test entities for ESP
	CEntity ent1;
	ent1.m_vecOrigin = Vector(256, 256, 50);
	Q_strncpy(ent1.m_szName, "Enemy1", sizeof(ent1.m_szName));
	ent1.m_bEnemy = true;
	m_Entities.AddToTail(ent1);

	CEntity ent2;
	ent2.m_vecOrigin = Vector(512, 512, 50);
	Q_strncpy(ent2.m_szName, "Enemy2", sizeof(ent2.m_szName));
	ent2.m_bEnemy = true;
	m_Entities.AddToTail(ent2);

	CEntity ent3;
	ent3.m_vecOrigin = Vector(128, 128, 50);
	Q_strncpy(ent3.m_szName, "Ally1", sizeof(ent3.m_szName));
	ent3.m_bEnemy = false;
	m_Entities.AddToTail(ent3);
}

void CWorldManager::DestroyEntities()
{
	g_pRenderManager->DestroyCameraProperty( m_PlayerEntity.m_pCameraProperty );
	m_Entities.RemoveAll();
}


//-----------------------------------------------------------------------------
// Gets the camera to world matrix
//-----------------------------------------------------------------------------
CPlayerEntity* CWorldManager::GetLocalPlayer()
{
	return &m_PlayerEntity;
}


//-----------------------------------------------------------------------------
// Sets the initial camera position
//-----------------------------------------------------------------------------
void CWorldManager::SetInitialLocalPlayerPosition()
{
	float flDistance = 1024.0;
	Vector vecCameraDirection( 1.0f, 1.0f, -0.5f );
	VectorNormalize( vecCameraDirection );

	m_PlayerEntity.m_vecPosition = Vector( 512, 512, 0 );
	m_PlayerEntity.m_angAngles = QAngle( vecCameraDirection );
	m_PlayerEntity.m_angCameraAngles = m_PlayerEntity.m_angAngles;

	if ( cl_thirdperson.GetBool() )
	{
		// Third person: camera behind player
		Vector forward;
		AngleVectors( m_PlayerEntity.m_angAngles, &forward );
		m_PlayerEntity.m_pCameraProperty->m_Origin = m_PlayerEntity.m_vecPosition - forward * 100.0f + Vector(0,0,50);
	}
	else
	{
		// First person: camera at player position
		m_PlayerEntity.m_pCameraProperty->m_Origin = m_PlayerEntity.m_vecPosition;
	}

	m_PlayerEntity.m_pCameraProperty->m_Angles = m_PlayerEntity.m_angCameraAngles;
}


//-----------------------------------------------------------------------------
// Draws the UI
//-----------------------------------------------------------------------------
void CWorldManager::DrawWorld()
{
	m_pHeightField->Draw( );
}


//-----------------------------------------------------------------------------
// Commands
//-----------------------------------------------------------------------------
void CWorldManager::ForwardStart( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;
	
	Vector vecForward;
	pCamera->GetForward( &vecForward );
	
	VectorMA( pCamera->m_Velocity, cam_forwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );

	// Update player position
	VectorMA( m_PlayerEntity.m_vecPosition, cam_forwardspeed.GetFloat() * 0.01f, vecForward, m_PlayerEntity.m_vecPosition );
}

void CWorldManager::ForwardStop( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;

	Vector vecForward;
	pCamera->GetForward( &vecForward );

	VectorMA( pCamera->m_Velocity, -cam_forwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );

	// Update player position
	VectorMA( m_PlayerEntity.m_vecPosition, -cam_forwardspeed.GetFloat() * 0.01f, vecForward, m_PlayerEntity.m_vecPosition );
}

void CWorldManager::BackwardStart( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;

	Vector vecForward;
	pCamera->GetForward( &vecForward );

	VectorMA( pCamera->m_Velocity, -cam_backwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );

	// Update player position
	VectorMA( m_PlayerEntity.m_vecPosition, -cam_backwardspeed.GetFloat() * 0.01f, vecForward, m_PlayerEntity.m_vecPosition );
}

void CWorldManager::BackwardStop( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;

	Vector vecForward;
	pCamera->GetForward( &vecForward );

	VectorMA( pCamera->m_Velocity, cam_backwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );

	// Update player position
	VectorMA( m_PlayerEntity.m_vecPosition, cam_backwardspeed.GetFloat() * 0.01f, vecForward, m_PlayerEntity.m_vecPosition );
}