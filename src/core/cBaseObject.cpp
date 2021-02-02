//----------------------------------------------------------------------------
//
// File:        cBaseObject.cpp
// Name:
// Programmer:  Marc Rousseau
// Date:        13-April-1998
//
// Description:
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <typeinfo>

#include "common.hpp"
#include "logger.hpp"
#include "iBaseObject.hpp"
#include "cBaseObject.hpp"

DBG_REGISTER( __FILE__ );

#if defined( DEBUG )

	cBaseObject::tBaseObjectMap cBaseObject::sm_ActiveObjects;

	void cBaseObject::Check( )
	{
		FUNCTION_ENTRY( nullptr, "Check", true );

		size_t count = sm_ActiveObjects.size( );

		if ( count != 0 )
		{
			DBG_ERROR ( "There " << (( count == 1 ) ? "is" : "are" ) << " still " << count << " active cBaseObject" << (( count != 1 ) ? "s" : "" ));
			for( auto object : sm_ActiveObjects )
			{
				object.second->DumpObject( "  " );
			}
		}
	}

	static int x = atexit( cBaseObject::Check );

#endif

cBaseObject::cBaseObject( const char *name ) :
#if defined( DEBUG )
		m_Tag( BASE_OBJECT_VALID ),
		m_ObjectCount( 0 ),
		m_ObjectName( name ),
		m_OwnerList( ),
#endif
	m_RefCount( 0 )
{
	FUNCTION_ENTRY( this, "cBaseObject ctor", true );

	DBG_TRACE( "New " << name << " object: " << this );

#if defined( DEBUG )
		DBG_ASSERT( name != nullptr );

		sm_ActiveObjects[ this ] = this;
#else
		UNREFERENCED_PARAMETER( name );
#endif
}

cBaseObject::~cBaseObject( )
{
	FUNCTION_ENTRY( this, "cBaseObject dtor", true );

#if defined( DEBUG )

		DBG_ASSERT( IsValid( ));

		// We should only get here through Release, but make sure anyway
		DBG_ASSERT( m_RefCount == 0 );

		// RefCount should match the OwnerList
		if( m_RefCount != m_OwnerList.size( ))
		{
			DBG_ERROR( m_ObjectName << " object " << static_cast< void *>( this ) << " RefCount discrepancy (" << m_RefCount << " vs " << m_OwnerList.size( ) << ")" );
		}

		if(( m_OwnerList.empty( ) == false ) || ( m_ObjectCount > 0 ))
		{
			DumpObject( "" );
		}

		m_Tag         = BASE_OBJECT_INVALID;
		m_ObjectName  = "<Deleted>";
		m_ObjectCount = 0;

		sm_ActiveObjects.erase( this );

#endif

	m_RefCount    = 0;
}

#if defined( DEBUG )

	void cBaseObject::DumpObject( const char *space ) const
	{
		FUNCTION_ENTRY( this, "cBaseObject::DumpObject", true );

#if defined( LOGGER )
		DBG_TRACE( space << m_ObjectName << " object " << const_cast< cBaseObject * >( this ) );

		if( m_ObjectCount > 0 )
		{
			DBG_TRACE( space << "  Owns " << m_ObjectCount << " object" << (( m_ObjectCount != 1 ) ? "s" : "" ));
		}

		if( m_OwnerList.empty( ) == false )
		{
			DBG_TRACE( space << "  Owned by " << m_OwnerList.size( ) << (( m_OwnerList.size( ) == 1 ) ? " object" : " objects" ));
			for( auto owner : m_OwnerList )
			{
				DBG_TRACE( space << "    " << ( owner ? owner->ObjectName( ) : "nullptr" ) << " object " << static_cast< void * >( owner ));
			}
		}
#endif
	}

	bool cBaseObject::CheckValid( ) const
	{
		FUNCTION_ENTRY( this, "cBaseObject::CheckValid", true );

		// We don't do anything more here - let derived classes to their thing
		return true;
	}

	bool cBaseObject::IsValid( ) const
	{
		FUNCTION_ENTRY( this, "cBaseObject::IsValid", true );

		// Wrap all member variable access inside the exception handler
		try
		{
			// See if we have a valid tag
			if( m_Tag != BASE_OBJECT_VALID )
			{
				return false;
			}

			// OK, it looks like one of ours, try using the virtual CheckValid method
			return CheckValid( );
		}
		catch( ... )
		{
		}

		return false;
	}

	void cBaseObject::AddObject( )
	{
		FUNCTION_ENTRY( this, "cBaseObject::AddObject", true );

		DBG_ASSERT( IsValid( ));

		m_ObjectCount++;
	}

	void cBaseObject::RemoveObject( )
	{
		FUNCTION_ENTRY( this, "cBaseObject::RemoveObject", true );

		DBG_ASSERT( IsValid( ));
		DBG_ASSERT( m_ObjectCount > 0 );

		m_ObjectCount--;
	}

	void cBaseObject::AddOwner( cBaseObject *owner )
	{
		FUNCTION_ENTRY( this, "cBaseObject::AddOwner", true );

		DBG_ASSERT( IsValid( ));

		m_OwnerList.push_back( owner );

		if( owner != nullptr )
		{
			owner->AddObject( );
		}
	}

	void cBaseObject::RemoveOwner( cBaseObject *owner )
	{
		FUNCTION_ENTRY( this, "cBaseObject::RemoveOwner", true );

		DBG_ASSERT( IsValid( ));

		tBaseObjectList::iterator ptr = std::find( m_OwnerList.begin( ), m_OwnerList.end( ), owner );

		if( ptr != m_OwnerList.end( ))
		{
			if( owner != nullptr )
			{
				owner->RemoveObject( );
			}
			m_OwnerList.erase( ptr );
			return;
		}

		DBG_ERROR(( owner ? owner->ObjectName( ) : "nullptr" ) << " object " << static_cast< void * >( owner ) <<
							" does not own " << m_ObjectName << " object " << static_cast< void * >( this ));
	}

	const std::string &cBaseObject::ObjectName( ) const
	{
		FUNCTION_ENTRY( this, "cBaseObject::ObjectName", true );

		return m_ObjectName;
	}

#endif

const void *cBaseObject::GetInterface( const std::string &iName ) const
{
	FUNCTION_ENTRY( this, "cBaseObject::GetInterface", false );

	if( iName == "iBaseObject" )
	{
		return static_cast< const iBaseObject * >( this );
	}

	return nullptr;
}

void *cBaseObject::GetInterface( const std::string &iName )
{
	FUNCTION_ENTRY( this, "cBaseObject::GetInterface", false );

	return const_cast< void * >( const_cast< const cBaseObject * >( this )->GetInterface( iName ));
}

size_t cBaseObject::AddRef( iBaseObject *obj )
{
	FUNCTION_ENTRY( this, "cBaseObject::AddRef", true );

	DBG_ASSERT( IsValid( ));

#if defined( DEBUG )
		try
{
        cBaseObject *base = dynamic_cast <cBaseObject *> ( obj );
			AddOwner( base );
    }
		catch( const std::bad_cast &er )
		{
        DBG_FATAL ( "Invalid cast: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
		catch( const std::exception &er )
		{
        DBG_FATAL ( "Exception: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
		catch( ... )
		{
        DBG_FATAL ( "Unexpected C++ exception!" );
    }
#else
		UNREFERENCED_PARAMETER( obj );
#endif

	return ++m_RefCount;
}

size_t cBaseObject::Release( iBaseObject *obj )
{
	FUNCTION_ENTRY( this, "cBaseObject::Release", true );

	DBG_ASSERT( IsValid( ));
	DBG_ASSERT( m_RefCount > 0 );

#if defined( DEBUG )
		try
		{
			cBaseObject *base = dynamic_cast< cBaseObject * >( obj );
			RemoveOwner( base );
        }
		catch( const std::bad_cast &er )
		{
			DBG_FATAL( "Invalid cast: Error casting " << typeid( obj ).name( ) << " (" << obj << ") : " << er.what( ));
		}
		catch( const std::exception &er )
		{
			DBG_FATAL( "Exception: Error casting " << typeid( obj ).name( ) << " (" << obj << ") : " << er.what( ));
		}
		catch( ... )
		{
            DBG_FATAL ( "Unexpected C++ exception!" );
        }
#else
		UNREFERENCED_PARAMETER( obj );
#endif

	size_t count = --m_RefCount;

	if( count == 0 )
	{
		try
		{
			delete this;
		}
		catch( ... )
		{
			DBG_FATAL( "Unexpected C++ exception!" );
		}
	}

	return count;
}
