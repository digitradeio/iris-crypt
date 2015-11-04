
// Copyright (c) 2010-2015 niXman (i dot nixman dog gmail dot com). All
// rights reserved.
//
// This file is part of YAS(https://github.com/niXman/yas) project.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef _yas_test__tuple_hpp__included_
#define _yas_test__tuple_hpp__included_

/***************************************************************************/

#include <yas/detail/tools/hexdumper.hpp>

#if defined(YAS_HAS_BOOST_TUPLE)
//#include <boost/tuple/tuple_comparison.hpp>
#endif // defined(YAS_HAS_BOOST_TUPLE)

template<typename archive_traits>
bool tuple_test(const char* archive_type, const char* io_type) {
	std::tuple<int, int> t5(4, 7), t6;

	typename archive_traits::oarchive oa3;
	archive_traits::ocreate(oa3, archive_type, io_type);
	oa3 & t5;

	typename archive_traits::iarchive ia3;
	archive_traits::icreate(ia3, oa3, archive_type, io_type);
	ia3 & t6;

	if ( t5 != t6 ) {
		std::cout << "TUPLE test failed! [3]" << std::endl;
		return false;
	}

	std::tuple<std::vector<std::string>> t7, t8;
	std::get<0>(t7).push_back("1");
	std::get<0>(t7).push_back("2");
	std::get<0>(t7).push_back("3");

	typename archive_traits::oarchive oa4;
	archive_traits::ocreate(oa4, archive_type, io_type);
	oa4 & t7;

	typename archive_traits::iarchive ia4;
	archive_traits::icreate(ia4, oa4, archive_type, io_type);
	ia4 & t8;

	if ( t7 != t7 ) {
		std::cout << "TUPLE test failed! [4]" << std::endl;
		return false;
	}

#if defined(YAS_HAS_BOOST_TUPLE)
	boost::tuples::tuple<int, int> t1(1, 3), t2;

	typename archive_traits::oarchive oa;
	archive_traits::ocreate(oa, archive_type, io_type);
	oa & t1;

	typename archive_traits::iarchive ia;
	archive_traits::icreate(ia, oa, archive_type, io_type);
	ia & t2;

	if ( t1 != t2 ) {
		std::cout << "TUPLE test failed! [1]" << std::endl;
		return false;
	}

	boost::tuples::tuple<std::vector<std::string>> t3, t4;
	boost::tuples::get<0>(t3).push_back("1");
	boost::tuples::get<0>(t3).push_back("2");
	boost::tuples::get<0>(t3).push_back("3");

	typename archive_traits::oarchive oa2;
	archive_traits::ocreate(oa2, archive_type, io_type);
	oa2 & t3;

	typename archive_traits::iarchive ia2;
	archive_traits::icreate(ia2, oa2, archive_type, io_type);
	ia2 & t4;

	if ( t3 != t4 ) {
		std::cout << "TUPLE test failed! [2]" << std::endl;
		return false;
	}
#endif // defined(YAS_HAS_BOOST_TUPLE)
	return true;
}

/***************************************************************************/

#endif // _yas_test__tuple_hpp__included_