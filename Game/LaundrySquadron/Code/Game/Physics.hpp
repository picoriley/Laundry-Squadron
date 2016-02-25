#pragma once
#include <vector>
#include <set>
#include "Engine/Math/Vector3.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Audio/Audio.hpp"
#include "Engine/Renderer/TheRenderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/AABB3.hpp"
#include "Engine/Renderer/Vertex.hpp"

//-----------------------------------------------------------------------------
enum ParticleType { /*PARTICLE_AABB2,*/ PARTICLE_AABB3, PARTICLE_SPHERE }; //FUTURE IDEAS TODO: Add whatever TheRenderer supports as a Draw call!


//-----------------------------------------------------------------------------
class LinearDynamicsState;


//-----------------------------------------------------------------------------
struct Force
{
public:

	virtual Vector3 CalcForceForStateAndMass( const LinearDynamicsState* lds, float mass ) const = 0;
	virtual Force* GetCopy() const = 0;


protected:

	Force( float magnitude, Vector3 direction = Vector3::ZERO ) 
		: m_magnitude( magnitude )
		, m_direction( direction ) 
	{
	}

	float m_magnitude;
	Vector3 m_direction;

	virtual float CalcMagnitudeForState( const LinearDynamicsState* /*lds*/ ) const { return m_magnitude; }
	virtual Vector3 CalcDirectionForState( const LinearDynamicsState* /*lds*/ ) const { return m_direction; }
};


//-----------------------------------------------------------------------------
struct GravityForce : public Force // m*g
{
	GravityForce()
		: Force( 9.81f, -Vector3::UP )
	{
	}
	GravityForce( float magnitude, Vector3 direction = -Vector3::UP)
		: Force( magnitude, direction )
	{
	}

	Vector3 CalcForceForStateAndMass( const LinearDynamicsState* lds, float mass ) const override;
	Force* GetCopy() const { return new GravityForce( *this ); }
};


//-----------------------------------------------------------------------------
struct DebrisForce : public Force //The real problem: can't directly drag down velocity to zero, as lds is const.
{
	DebrisForce()
		: Force( 9.81f, -Vector3::UP), m_groundHeight( 0.f )
	{
	}
	DebrisForce( float magnitude, float groundHeight = 0.f, Vector3 direction = -Vector3::UP)
		: Force( magnitude, direction ), m_groundHeight( groundHeight )
	{
	}

	float m_groundHeight;

	Vector3 CalcForceForStateAndMass( const LinearDynamicsState* lds, float mass ) const override;
	float CalcMagnitudeForState( const LinearDynamicsState* lds ) const override; //Magnitude shrinks if you hit/sink below ground.
	virtual Vector3 CalcDirectionForState( const LinearDynamicsState* lds ) const override; //Direction inverts if you hit/sink below ground.
	Force* GetCopy() const { return new DebrisForce( *this ); }
};


//-----------------------------------------------------------------------------
struct ConstantWindForce : public Force // -c*(v - w)
{
	ConstantWindForce( float magnitude, Vector3 direction, float dampedness = 1.0f ) 
		: Force( magnitude, direction )
		, m_dampedness( dampedness ) 
	{ 
	}

	float m_dampedness; //"c".

	Vector3 CalcForceForStateAndMass( const LinearDynamicsState* lds, float mass ) const override;
	Force* GetCopy() const { return new ConstantWindForce( *this ); }
};


//-----------------------------------------------------------------------------
struct WormholeForce : public Force // -c*(v - w(pos))
{
	WormholeForce( float magnitude, Vector3 direction, float dampedness = 1.0f ) 
		: Force( magnitude, direction )
		, m_dampedness( dampedness ) 
	{
	}

	float m_dampedness; //"c".
	
	//Overriding to make wind = wind(pos).
	float CalcMagnitudeForState( const LinearDynamicsState* lds ) const override; //Further from origin you move == stronger the wind.
	virtual Vector3 CalcDirectionForState( const LinearDynamicsState* lds ) const override; //Direction sends you back toward origin.
	Vector3 CalcForceForStateAndMass( const LinearDynamicsState* lds, float mass ) const override;
	Force* GetCopy() const { return new WormholeForce( *this ); }
};


//-----------------------------------------------------------------------------
struct SpringForce : public Force // -cv + -kx
{
	SpringForce( float magnitude, Vector3 direction, float stiffness, float dampedness = 1.0f ) 
		: Force( magnitude, direction )
		, m_dampedness( dampedness )
		, m_stiffness( stiffness )
	{ 
	}

	float m_dampedness; //"c".
	float m_stiffness; //"k".

	Vector3 CalcForceForStateAndMass( const LinearDynamicsState* lds, float mass ) const override;
	Force* GetCopy() const { return new SpringForce( *this ); }
};


//-----------------------------------------------------------------------------
class LinearDynamicsState //Question: could these be attached to an EntityNode[3D]'s, and engine just make all entities + physics run in 3D?
{
public:

	LinearDynamicsState( Vector3 position = Vector3::ZERO, Vector3 velocity = Vector3::ZERO )
		: m_position( position )
		, m_velocity( velocity )
	{
	}
	~LinearDynamicsState();

	void StepWithForwardEuler( float mass, float deltaSeconds );
	void StepWithVerlet( float mass, float deltaSeconds );
	Vector3 GetPosition() const { return m_position; }
	Vector3 GetVelocity() const { return m_velocity; }
	void SetPosition( const Vector3& newPos ) { m_position = newPos; }
	void SetVelocity( const Vector3& newVel ) { m_velocity = newVel; }
	void AddForce( Force* newForce ) { m_forces.push_back( newForce ); }
	void GetForces( std::vector< Force* >& out_forces ) { out_forces = m_forces; }


private:

	Vector3 m_position;
	Vector3 m_velocity;
	std::vector<Force*> m_forces; //i.e. All forces acting on whatever this LDS is attached to.

	LinearDynamicsState dStateForMass( float mass ) const; //Solves accel, for use in Step() integrators.
	Vector3 CalcNetForceForMass( float mass ) const; //Used by dState(), gets Sigma[F] by looping m_forces.
};


//-----------------------------------------------------------------------------
class Particle //NOTE: NOT an Entity3D because some entities won't need expiration logic.
{
public:

	Particle( ParticleType renderType, float mass, float secondsToLive, float renderRadius )
		: m_mass( mass )
		, m_secondsToLive( secondsToLive )
		, m_renderType( renderType )
		, m_renderRadius( renderRadius )
		, m_state( nullptr )
		, m_isPinned( false )
	{
	}
	~Particle();

	void GetParticleState(LinearDynamicsState& out_state) const { out_state = *m_state; }
	void SetParticleState( LinearDynamicsState* newState ) { m_state = newState; }
	void Render();
	void StepAndAge( float deltaSeconds );
	void SetIsExpired( bool newVal ) { m_secondsToLive = newVal ? -1.f : 1.f; }
	bool IsExpired() const { return m_secondsToLive <= 0.f; }
	void GetForces( std::vector< Force* >& out_forces ) const;
	void AddForce( Force* newForce );
	void CloneForcesFromParticle( const Particle* sourceParticle );				
	
	//The return bool value indicates failure <=> particle has no m_state.
	bool GetPosition( Vector3& out_position );
	bool SetPosition( const Vector3& newPosition );
	bool Translate( const Vector3& newPosition );
	bool GetVelocity( Vector3& out_velocity );
	bool SetVelocity( const Vector3& newVelocity );

	bool GetIsPinned() const { return m_isPinned; }
	void SetIsPinned( bool newVal ) { m_isPinned = newVal; }
	void ToggleIsPinned() { m_isPinned = !m_isPinned; }

	LinearDynamicsState* m_state;


private:

	bool m_isPinned;

	float m_mass;
	float m_secondsToLive;

	ParticleType m_renderType;
	float m_renderRadius;

};


//-----------------------------------------------------------------------------
class ParticleSystem
{
public:

	ParticleSystem( Vector3 emitterPosition, ParticleType particleType, float particleRadius, float particleMass, 
					float muzzleSpeed, float maxDegreesDownFromWorldUp, float minDegreesDownFromWorldUp, float maxDegreesLeftFromWorldNorth, float minDegreesLeftFromWorldNorth,
					float secondsBetweenEmits, float secondsBeforeParticlesExpire, unsigned int maxParticlesEmitted, unsigned int particlesEmittedAtOnce )
		: m_emitterPosition( emitterPosition )
		, m_muzzleSpeed( muzzleSpeed )
		, m_maxDegreesDownFromWorldUp( maxDegreesDownFromWorldUp )
		, m_minDegreesDownFromWorldUp( minDegreesDownFromWorldUp )
		, m_maxDegreesLeftFromWorldNorth( maxDegreesLeftFromWorldNorth )
		, m_minDegreesLeftFromWorldNorth( minDegreesLeftFromWorldNorth )
		, m_particleToEmit( particleType, particleMass, secondsBeforeParticlesExpire, particleRadius )
		, m_secondsBetweenEmits( secondsBetweenEmits )
		, m_secondsBeforeParticlesExpire( secondsBeforeParticlesExpire )
		, m_maxParticlesEmitted( maxParticlesEmitted )
		, m_particlesEmittedAtOnce( particlesEmittedAtOnce )
		, m_secondsPassedSinceLastEmit( 0.f )
	{
		GUARANTEE_OR_DIE( m_particlesEmittedAtOnce <= m_maxParticlesEmitted, "Error in ParticleSystem ctor, amount to emit at once exceeds max amount to emit." ); //Else infinite loop in EmitParticles().
		m_particleToEmit.SetParticleState( new LinearDynamicsState( emitterPosition, Vector3::ZERO ) ); //So we can add forces to it prior to emission if requested.
		ParticleSystem::s_emitSoundID = AudioSystem::instance->CreateOrGetSound( "Data/Audio/Explo_EnergyFireball01.wav" );
	}
	~ParticleSystem();

	void RenderThenExpireParticles();
	void UpdateParticles( float deltaSeconds );
	void AddForce( Force* newForce ) { m_particleToEmit.AddForce( newForce ); }
	float GetSecondsUntilNextEmit() const { return m_secondsBetweenEmits - m_secondsPassedSinceLastEmit;  }

private:

	void StepAndAgeParticles( float deltaSeconds );
	void EmitParticles( float deltaSeconds ); //silently emits nothing if not yet time to emit.

	float m_maxDegreesDownFromWorldUp; //"theta" in most spherical-to-Cartesian conversions.
	float m_minDegreesDownFromWorldUp;
	float m_maxDegreesLeftFromWorldNorth; //"phi".
	float m_minDegreesLeftFromWorldNorth; 

	float m_muzzleSpeed; //How fast particles shoot out.
	float m_secondsPassedSinceLastEmit;
	float m_secondsBetweenEmits;
	float m_secondsBeforeParticlesExpire;
	unsigned int m_maxParticlesEmitted;
	unsigned int m_particlesEmittedAtOnce; //Destroys oldest one(s) on next emit until emitter can emit this amount. 
	//No angular velocity right now.
	//No ability to ignore parent velocity right now.

	Vector3 m_emitterPosition;

	Particle m_particleToEmit;
	std::vector< Particle* > m_unexpiredParticles;

	static const Vector3 MAX_PARTICLE_OFFSET_FROM_EMITTER;
	static SoundID s_emitSoundID;
};


//-----------------------------------------------------------------------------
enum ConstraintType { STRETCH, SHEAR, BEND };
struct ClothConstraint
{
	ConstraintType type;
	Particle* const p1;
	Particle* const p2;
	double restDistance; //How far apart p1, p2 are when cloth at rest.
	ClothConstraint( ConstraintType type, Particle* const p1, Particle* const p2, double restDistance )
		: type( type ), p1( p1 ), p2( p2 ), restDistance( restDistance ) {}
};


//-----------------------------------------------------------------------------
class Cloth

{
public:
	//CONTRUCTORS//////////////////////////////////////////////////////////////////////////
	Cloth( const Vector3& originTopLeftPosition,
		   ParticleType particleRenderType, float particleMass, float particleRadius,
		   int numRows, int numCols,
		   unsigned int numConstraintSolverIterations,
		   double baseDistanceBetweenParticles,
		   double ratioDistanceStructuralToShear,
		   double ratioDistanceStructuralToBend,
		   const Vector3& initialGlobalVelocity = Vector3::ZERO )
		: m_originalTopLeftPosition( originTopLeftPosition )
		, m_currentTopLeftPosition( originTopLeftPosition )
		, m_numRows( numRows )
		, m_numCols( numCols )
		, m_numConstraintSolverIterations( numConstraintSolverIterations )
		, m_baseDistanceBetweenParticles( baseDistanceBetweenParticles )
		, m_ratioDistanceStructuralToShear( ratioDistanceStructuralToShear )
		, m_ratioDistanceStructuralToBend( ratioDistanceStructuralToBend )
		, m_particleTemplate( particleRenderType, particleMass, -1.f, particleRadius )
	{
		m_clothParticles.reserve( numRows * numCols );
		for ( int i = 0; i < numRows * numCols; i++ )
			m_clothParticles.push_back( Particle( particleRenderType, particleMass, 1.f, particleRadius ) ); //Doesn't assign a dynamics state.
				//Needs a positive secondsToLive or else expiration logic will say it's already invisible/dead.

		AssignParticleStates( static_cast<float>( baseDistanceBetweenParticles ), originTopLeftPosition.y, initialGlobalVelocity );

		AddConstraints( baseDistanceBetweenParticles, ratioDistanceStructuralToShear, ratioDistanceStructuralToBend );

		GetParticle( 0, 0 )->SetIsPinned( true );
		GetParticle( 0, numCols - 1 )->SetIsPinned( true );
	}
	~Cloth() { for ( ClothConstraint* cc : m_clothConstraints ) delete cc; }

	//FUNCTIONS//////////////////////////////////////////////////////////////////////////
	Particle* const GetParticle( int rowStartTop, int colStartLeft )
	{
		if ( rowStartTop > m_numRows )
			return nullptr;
		if ( colStartLeft > m_numCols )
			return nullptr;
		return &m_clothParticles[ ( rowStartTop * m_numRows ) + colStartLeft ]; //Row-major.
	}

	//-----------------------------------------------------------------------------------
	void Update( float deltaSeconds )
	{
		float fixedTimeStep = .001f;

		for ( int particleIndex = 0; particleIndex < m_numRows * m_numCols; particleIndex++ )
			if ( ( m_clothParticles[ particleIndex ].GetIsPinned() == false ) || m_clothParticles[ particleIndex ].IsExpired() ) //What happens if you add if ( isExpired() ) ?
				m_clothParticles[ particleIndex ].StepAndAge( fixedTimeStep );

		//In future could remove this to a RemoveConstraintForParticle(Particle* p) that finds and erases all constraints referencing p, to not loop per frame.
		for ( auto constraintIter = m_clothConstraints.begin(); constraintIter != m_clothConstraints.end(); )
		{
			ClothConstraint* cc = *constraintIter;
			if ( cc->p1->IsExpired() && cc->p2->IsExpired() )
			{
				constraintIter = m_clothConstraints.erase( constraintIter );
			}
			else
			{
				++constraintIter;
			}
		}

		SatisfyConstraints( fixedTimeStep );

		//Old way of pinning the corners. Now handled by Particle::m_isPinned member to let you pin things arbitrarily.

//		if ( GetParticle( 0, 0 )->IsExpired() == false )
//			GetParticle( 0, 0 )->SetPosition( m_currentTopLeftPosition );
//		if ( GetParticle( 0, m_numCols - 1 )->IsExpired() == false )
//			GetParticle( 0, m_numCols - 1 )->SetPosition( CalcTopRightPosFromTopLeft() );
	}

	//-----------------------------------------------------------------------------------
	void Render( bool showCloth = true, bool showConstraints = false, bool showParticles = false )
	{
		//Render the cloth "fabric" by taking every 4 particle positions (r,c) to (r+1,c+1) in to make a quad.
		Vector3 particleStateTopLeft; //as 0,0 is top left. 
		Vector3 particleStateTopRight;
		Vector3 particleStateBottomLeft;
		Vector3 particleStateBottomRight;
		AABB3 bounds;

		if ( showCloth )
		{
			for ( int r = 0; ( r + 1 ) < m_numRows; r++ )
			{
				for ( int c = 0; ( c + 1 ) < m_numCols; c++ )
				{
					if ( GetParticle( r, c )->IsExpired() && GetParticle( r, c )->IsExpired() && GetParticle( r, c )->IsExpired() && GetParticle( r, c )->IsExpired() )
						continue; //Don't draw a quad for a particle that's been shot.

					GetParticle( r, c )->GetPosition( particleStateTopLeft );
					GetParticle( r, c + 1 )->GetPosition( particleStateTopRight );
					GetParticle( r + 1, c )->GetPosition( particleStateBottomLeft );
					GetParticle( r + 1, c + 1 )->GetPosition( particleStateBottomRight );

					Vector2 currentU = Vector2::UNIT_X - (Vector2::UNIT_X * (((float)(c + 1) / (float)(m_numCols - 1))));
					Vector2 currentV = Vector2::UNIT_Y * ((float)r / (float)(m_numRows - 1));
					Vector2 nextU = Vector2::UNIT_X - (Vector2::UNIT_X * (((float)c / (float)(m_numCols - 1))));
					Vector2 nextV = Vector2::UNIT_Y * ((float)(r + 1) / (float)(m_numRows - 1));
					Vertex_PCT quad[ 4 ] =
					{
						Vertex_PCT( particleStateBottomLeft, RGBA::WHITE, nextU + nextV),
						Vertex_PCT( particleStateBottomRight, RGBA::WHITE, currentU + nextV ),
						Vertex_PCT( particleStateTopRight, RGBA::WHITE, currentU + currentV ),
						Vertex_PCT( particleStateTopLeft, RGBA::WHITE, nextU + currentV )
					};
					TheRenderer::instance->DrawVertexArray( quad, 4, TheRenderer::QUADS, Texture::CreateOrGetTexture("Data/Images/Test.png")); //Can't use AABB, cloth quads deform from being axis-aligned.
				}
			}
		}

		if ( showConstraints )
		{
			for ( ClothConstraint* cc : m_clothConstraints )
			{
				Vector3 particlePosition1;
				Vector3 particlePosition2;
				cc->p1->GetPosition( particlePosition1 );
				cc->p2->GetPosition( particlePosition2 );

				switch ( cc->type )
				{
				case STRETCH:	TheRenderer::instance->DrawLine( particlePosition1, particlePosition2, RGBA::RED ); break;
				case SHEAR:		TheRenderer::instance->DrawLine( particlePosition1, particlePosition2, RGBA::GREEN ); break;
				case BEND:		TheRenderer::instance->DrawLine( particlePosition1, particlePosition2, RGBA::BLUE ); break;
				}
			}
		}

		if ( !showParticles )
			return;

		for ( int particleIndex = 0; particleIndex < m_numRows * m_numCols; particleIndex++ )
			if ( m_clothParticles[ particleIndex ].IsExpired() == false )
				m_clothParticles[ particleIndex ].Render();
	}

	//-----------------------------------------------------------------------------------
	inline void MoveClothByOffset(const Vector3& offset) 
	{
		for (int c = 0; c < m_numCols; c++)
		{
			Vector3 currentPosition;
			Particle* currentParticle = GetParticle(0, c);
			currentParticle->GetPosition(currentPosition);
			currentParticle->SetPosition(currentPosition + offset);
		}
		GetParticle(0, 0)->GetPosition(m_currentTopLeftPosition);
	}
	
	//-----------------------------------------------------------------------------------
	inline Vector3 GetCurrentTopLeftPosition()
	{
		return m_currentTopLeftPosition;
	}

	//-----------------------------------------------------------------------------------
	inline Vector3 GetOriginalTopLeftPosition()
	{
		return m_originalTopLeftPosition;
	}

	//-----------------------------------------------------------------------------------
	inline void SetTopLeftPosition(const Vector3& offset)
	{
		m_currentTopLeftPosition = offset;
	}
private:
	//-----------------------------------------------------------------------------------
	void AssignParticleStates( float baseDistance, float nonPlanarDepth, const Vector3& velocity = Vector3::ZERO ) //Note: 0,0 == top-left, so +x is right, +y is down.
	{
		//FORCES ASSIGNED HERE RIGHT NOW:
		LinearDynamicsState* lds = new LinearDynamicsState(); //Need its forces to stay valid over cloth lifetime, particle will handle cleanup.
		m_particleTemplate.SetParticleState( lds );
		m_particleTemplate.AddForce( new GravityForce( 9.81f, Vector3(0,0,-1) ) );
		//m_particleTemplate.AddForce( new SpringForce( 0, Vector3::ZERO, .72f, .72f ) );
		//m_particleTemplate.AddForce( new ConstantWindForce( 1.f, WORLD_RIGHT ) );

		for ( int r = 0; r < m_numRows; r++ )
		{
			for ( int c = 0; c < m_numCols; c++ )
			{
				Vector3 startPosition(c * baseDistance, 0.0f, -r * baseDistance ); //BASIS CHANGE GOES HERE!
				startPosition += m_currentTopLeftPosition;
				Particle* const currentParticle = GetParticle( r, c );

				currentParticle->SetParticleState( new LinearDynamicsState( startPosition, velocity ) ); //Particle will handle state cleanup.
				currentParticle->CloneForcesFromParticle( &m_particleTemplate );
			}
		}
	}

	//-----------------------------------------------------------------------------------
	Vector3 CalcTopRightPosFromTopLeft()
	{
		m_currentTopRightPosition = m_currentTopLeftPosition;
		m_currentTopRightPosition.x += ( ( m_numCols - 1 ) * static_cast<float>( m_baseDistanceBetweenParticles ) ); //Might need to change direction per engine basis.
		return m_currentTopRightPosition;
	}

	//-----------------------------------------------------------------------------------
	void SetDistancesForConstraints( ConstraintType affectedType, double newRestDistance )
	{
		for ( unsigned int constraintIndex = 0; constraintIndex < m_clothConstraints.size(); constraintIndex++ )
			if ( m_clothConstraints[ constraintIndex ]->type == affectedType )
				m_clothConstraints[ constraintIndex ]->restDistance = newRestDistance;
	}

	//-----------------------------------------------------------------------------------
	void AddConstraints( double baseDistance, double ratioStructuralToShear, double ratioStructuralToBend )
	{
		double shearDist = baseDistance * ratioStructuralToShear;
		double bendDist = baseDistance * ratioStructuralToBend;

		std::set<ClothConstraint*> tmpSet;

		for ( int r = 0; r < m_numRows; r++ )
		{
			for ( int c = 0; c < m_numCols; c++ )
			{
				if ( ( r + 1 ) < m_numRows )
					tmpSet.insert( new ClothConstraint( STRETCH, GetParticle( r, c ), GetParticle( r + 1, c ), baseDistance ) );
				if ( ( r - 1 ) >= 0 )
					tmpSet.insert( new ClothConstraint( STRETCH, GetParticle( r, c ), GetParticle( r - 1, c ), baseDistance ) );
				if ( ( c + 1 ) < m_numCols )
					tmpSet.insert( new ClothConstraint( STRETCH, GetParticle( r, c ), GetParticle( r, c + 1 ), baseDistance ) );
				if ( ( c - 1 ) >= 0 )
					tmpSet.insert( new ClothConstraint( STRETCH, GetParticle( r, c ), GetParticle( r, c - 1 ), baseDistance ) );

				if ( ( r + 1 ) < m_numRows && ( c + 1 ) < m_numCols )
					tmpSet.insert( new ClothConstraint( SHEAR, GetParticle( r, c ), GetParticle( r + 1, c + 1 ), shearDist ) );
				if ( ( r - 1 ) >= 0 && ( c + 1 ) < m_numCols )
					tmpSet.insert( new ClothConstraint( SHEAR, GetParticle( r, c ), GetParticle( r - 1, c + 1 ), shearDist ) );
				if ( ( r + 1 ) < m_numRows && ( c - 1 ) >= 0 )
					tmpSet.insert( new ClothConstraint( SHEAR, GetParticle( r, c ), GetParticle( r + 1, c - 1 ), shearDist ) );
				if ( ( r - 1 ) >= 0 && ( c - 1 ) >= 0 )
					tmpSet.insert( new ClothConstraint( SHEAR, GetParticle( r, c ), GetParticle( r - 1, c - 1 ), shearDist ) );


				if ( ( r + 2 ) < m_numRows )
					tmpSet.insert( new ClothConstraint( BEND, GetParticle( r, c ), GetParticle( r + 2, c ), bendDist ) );
				if ( ( r - 2 ) >= 0 )
					tmpSet.insert( new ClothConstraint( BEND, GetParticle( r, c ), GetParticle( r - 2, c ), bendDist ) );
				if ( ( c + 2 ) < m_numCols )
					tmpSet.insert( new ClothConstraint( BEND, GetParticle( r, c ), GetParticle( r, c + 2 ), bendDist ) );
				if ( ( c - 2 ) >= 0 )
					tmpSet.insert( new ClothConstraint( BEND, GetParticle( r, c ), GetParticle( r, c - 2 ), bendDist ) );
			}
		}

		//Now that we know it's duplicate-free, store in the vector to get the ability to index.
		for ( ClothConstraint* cc : tmpSet )
			m_clothConstraints.push_back( cc );
	}

	//-----------------------------------------------------------------------------------
	void SatisfyConstraints( float deltaSeconds )
	{
		double norm = 0.0;
		for ( unsigned int numIteration = 0; numIteration < m_numConstraintSolverIterations; ++numIteration )
		{
			for ( unsigned int constraintIndex = 0; constraintIndex < m_clothConstraints.size(); constraintIndex++ )
			{
				ClothConstraint* currentConstraint = m_clothConstraints[ constraintIndex ];

				Vector3 particlePosition1;
				Vector3 particlePosition2;

				currentConstraint->p1->GetPosition( particlePosition1 );
				currentConstraint->p2->GetPosition( particlePosition2 );

				Vector3 currentDisplacement = particlePosition2 - particlePosition1;

				if ( currentDisplacement == Vector3::ZERO )
					continue; //Skip solving for a step.
				double currentDistance = currentDisplacement.CalculateMagnitude();

				float stiffness = 100.f;
				Vector3 halfCorrectionVector = currentDisplacement * stiffness * static_cast<float>( 0.5 * ( 1.0 - ( currentConstraint->restDistance / currentDistance ) ) );
				// Note last term is ( currDist - currConstraint.restDist ) / currDist, just divided through.

				norm += (currentConstraint->restDistance - currentDistance) * (currentConstraint->restDistance - currentDistance);

				//Move p2 towards p1 (- along halfVec), p1 towards p2 (+ along halfVec).
				bool isPinnedParticle1 = currentConstraint->p1->GetIsPinned();
				bool isPinnedParticle2 = currentConstraint->p2->GetIsPinned();

// 				if ( isPinnedParticle1 && isPinnedParticle2 )
// 				{
// 					return; //Neither need correction.
// 				}
// 				if ( isPinnedParticle1 && !isPinnedParticle2 )
// 				{
// 					currentConstraint->p2->Translate( -halfCorrectionVector * 1.f * deltaSeconds ); //Have to cover the full correction with one particle.
// 					return;
// 				}
// 				if ( !isPinnedParticle1 && isPinnedParticle2 )
// 				{
// 					currentConstraint->p1->Translate( halfCorrectionVector * 1.f * deltaSeconds ); //Have to cover the full correction with one particle.
// 					return;
// 				}

				//Neither point is pinned, correct as normal.
				if ( !isPinnedParticle1 )
				currentConstraint->p1->Translate( halfCorrectionVector * ( isPinnedParticle2 ? 2.f : 1.f ) * deltaSeconds );
				
				if ( !isPinnedParticle2 )
					currentConstraint->p2->Translate( -halfCorrectionVector * ( isPinnedParticle1 ? 2.f : 1.f ) *deltaSeconds );
			}
		}
		DebuggerPrintf("Error: %f\n", norm);
	}

	//MEMBER VARIABLES//////////////////////////////////////////////////////////////////////////
	Particle m_particleTemplate; //Without this and CloneForces, adding forces will crash when they go out of scope.
	Vector3 m_originalTopLeftPosition;
	Vector3 m_currentTopLeftPosition; //m_clothParticles[0,0].position: MOVE THIS WITH WASD TO MOVE PINNED CORNERS!
	Vector3 m_currentTopRightPosition; //Update whenever WASD event occurs as an optimization, else just recalculating per-tick.
	int m_numRows;
	int m_numCols;
	unsigned int m_numConstraintSolverIterations; //Affects soggy: more is less sag.

	//Ratios stored with class mostly for debugging. Or maybe use > these to tell when break a cloth constraint?
	double m_baseDistanceBetweenParticles;
	double m_ratioDistanceStructuralToShear;
	double m_ratioDistanceStructuralToBend;

	std::vector<ClothConstraint*> m_clothConstraints; //TODO: make c-style after getting fixed-size formula given cloth dims?
public:
	std::vector<Particle> m_clothParticles; //A 1D array, use GetParticle for 2D row-col interfacing accesses. Vector in case we want to push more at runtime.
};
