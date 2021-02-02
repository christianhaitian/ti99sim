//----------------------------------------------------------------------------
//
// File:	cBaseObject.hpp
// Name:	
// Programmer:	Marc Rousseau
// Date:      	13-April-1998
//
// Description:
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef CBASEOBJECT_HPP_
#define CBASEOBJECT_HPP_

#include "common.hpp"
#include "iBaseObject.hpp"

#include <atomic>
#include <vector>
#include <map>

#define BASE_OBJECT_VALID       0x4556494C	// "LIVE"
#define BASE_OBJECT_INVALID     0x44414544	// "DEAD"

class cBaseObject :
	public virtual iBaseObject
{
#if defined( DEBUG )

	public:

		static void Check ();

		// NOTE: This function *CANNOT* be virtual!
		bool IsValid( ) const;

	protected:

		typedef std::vector<cBaseObject *> tBaseObjectList;
		typedef std::map<cBaseObject *,cBaseObject *> tBaseObjectMap;

		static tBaseObjectMap sm_ActiveObjects;

		UINT64              m_Tag;
		std::atomic<size_t>	m_ObjectCount;		// # of objects referenced by this object
		std::string         m_ObjectName;		// Name of most-derived Class
		tBaseObjectList     m_OwnerList;

		const std::string &ObjectName( ) const;

		void AddObject( );
		void RemoveObject( );

		void AddOwner( cBaseObject * );
		void RemoveOwner( cBaseObject * );

		virtual bool CheckValid( ) const;
		void DumpObject( const char * ) const;

#endif

	std::atomic<size_t>		m_RefCount;			// # of references on this object

private:

	// Disable the copy constructor and assignment operator defaults
	cBaseObject( const cBaseObject & ) = delete;				// no implementation
	cBaseObject &operator =( const cBaseObject & ) = delete;	// no implementation

protected:

	// We don't want anyone creating/deleting these directly - all derived classes should do the same
	cBaseObject( ) = default;
	cBaseObject( const char *name );
	virtual ~cBaseObject( ) override;

public:

	// iBaseObject Methods
	virtual const void *GetInterface( const std::string &name ) const override;
	virtual void *GetInterface( const std::string &name ) override;
	virtual size_t AddRef( iBaseObject * = nullptr ) override;
	virtual size_t Release( iBaseObject * = nullptr ) override;

};

#endif
