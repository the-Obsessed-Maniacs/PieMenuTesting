/******************************************************************************
 * File:        SelfRegFactory.h
 * Author:      Stefan <St0fF> Kaps
 * Copyleft:    2024, the obsessed Maniacs
 ******************************************************************************
 *  Using the idea published by Nir Friedman
 *  http://www.nirfriedman.com/2018/04/29/unforgettable-factory/
 *  to provide the means to easily create new helper classes that may
 *  implement different algorithms or whatever.
 *****************************************************************************/
#pragma once

#include <QDebug>

#pragma region Factory-Boilerplate
// Factory with automatic registration - boilerplate:
template < class Base >
class Factory
{
  public:
	using FuncType	= Base* ( * ) ();
	using EntryType = QPair< FuncType, QString >;
	using ListType	= QList< EntryType >;

	template < class... T >
	static Base* newT( int id, T&&... args )
	{
		return classes().at( id ).first();
	}
	static inline QString		  name( int id ) { return classes().at( id ).second; }
	static inline int			  count() { return classes().count(); }
	static inline const ListType& Classes() { return classes(); }

	template < class T >
	struct Registrar : Base
	{
		friend T;
		static bool registerT()
		{
			Factory::classes().append( { []() -> Base* { return new T; }, typeid( T ).name() } );
			qInfo() << "registered" << typeid( T ).name();
			return true;
		}
		static bool registered;

	  private:
		Registrar()
			: Base( Key{} )
		{
			( void ) registered;
		}
	};

	friend Base;

  private:
	Factory() = default;
	static ListType& classes()
	{
		static ListType s;
		return s;
	}
	class Key
	{
		Key() {}
		template < class T >
		friend struct Registrar;
	};
}; // namespace QGI

template < class Base >
template < class T >
bool Factory< Base >::Registrar< T >::registered = Factory< Base >::Registrar< T >::registerT();
#pragma endregion

#pragma region Base_Definition_Explanation
/**************************************************************************************************
 * A base class definition needs to follow a specific style to work.  Following this style, all
 * classes you then derive from the Definition's Registrar will be included in the factory.
 *
 * Define a struct as the Base like the following.  This may be followed by a type definition to get
 * rid of the template-boilerplate as shown.  The "additionalTypes*" type argument is a list of CTor
 * Parameters, which will be forwarded by the factory upon incance-create.
 *
 * Insert further pure virtual functions to actually declare the interface.
 *************************************************************************************************/
// struct BasisDefinition : Factory< BasisDefinition >
//{
//	BasisDefinition( Key ) {}
//	// define some interface here
// };
// typedef Factory< BasisDefinition > BasisFactory;
#pragma endregion

#pragma region Derived_Classes_Definition_Explanation
/**************************************************************************************************
 * To derive a class from the Base, you need to follow the following style.  This will automatically
 * register the class in the factory.  The class must have a private constructor, which is needed to
 * prevent the class from being instantiated directly, so only the factory can create instances.
 *
 *************************************************************************************************/
// class Derived : public BasisFactory::Registrar< Derived >
//{
//   private:
//	Derived()
//		: BasisFactory::Registrar< Derived >()
//	{}
//
//   public:
//	// Implement the interface here ...
// };
#pragma endregion
