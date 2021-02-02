//----------------------------------------------------------------------------
//
// File:	iBaseObject.hpp
// Name:
// Programmer:	Marc Rousseau
// Date:		13-April-1998
//
// Description:
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef IBASEOBJ_HPP_
#define IBASEOBJ_HPP_

#include <string>

struct iBaseObject
{
	virtual const void *GetInterface( const std::string &name ) const = 0;
	virtual void *GetInterface( const std::string &name ) = 0;
	virtual size_t AddRef( iBaseObject *owner = nullptr ) = 0;
	virtual size_t Release( iBaseObject *owner = nullptr ) = 0;

protected:

	iBaseObject( ) = default;
	iBaseObject( const iBaseObject & ) = delete;
	iBaseObject &operator =( const iBaseObject & ) = delete;

	virtual ~iBaseObject( ) = default;
};

template<class Type> class cRefPtr
{
private:

	Type		*m_pObject;

public:

	cRefPtr( ) :
		m_pObject( nullptr )
	{
	}

	cRefPtr( cRefPtr &&ptr ) :
		m_pObject( ptr.m_pObject )
	{
		ptr.m_pObject = nullptr;
	}

	cRefPtr( const cRefPtr &ptr ) :
		m_pObject( ptr.m_pObject )
	{
		if( m_pObject && ( &ptr != this ))
		{
			m_pObject->AddRef( );
		}
	}

	cRefPtr( Type *pInterface ) :
		m_pObject( pInterface )
	{
		if( m_pObject )
		{
			m_pObject->AddRef( );
		}
	}

	~cRefPtr( )
	{
		if( m_pObject )
		{
			m_pObject->Release( );
			m_pObject = nullptr;
		}
	}

	cRefPtr &operator =( cRefPtr &&rhs )
	{
		if( m_pObject )
		{
			m_pObject->Release( );
		}

		m_pObject = rhs.m_pObject;
		rhs.m_pObject = nullptr;

		return *this;
	}

	cRefPtr &operator =( const cRefPtr &rhs )
	{
		if( &rhs != this )
		{
			if( m_pObject )
			{
				m_pObject->Release( );
			}

			m_pObject = rhs.m_pObject;

			if( m_pObject )
			{
				m_pObject->AddRef( );
			}
		}

		return *this;
	}

	cRefPtr &operator =( Type *pInstance )
	{
		if( m_pObject )
		{
			m_pObject->Release( );
		}

		m_pObject = pInstance;

		if( m_pObject )
		{
			m_pObject->AddRef( );
		}

		return *this;
	}

	Type &operator *( ) const
	{
		return *m_pObject;
	}

	Type *operator ->( ) const
	{
		return m_pObject;
	}

	operator Type *( ) const
	{
		return m_pObject;
	}

	Type *get( )
	{
		return m_pObject;
	}

};

#endif
