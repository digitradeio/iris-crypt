
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

#include <iostream>
#include <fstream>
#include <cstdio>

#include <yas/binary_oarchive.hpp>
#include <yas/binary_iarchive.hpp>
#include <yas/text_oarchive.hpp>
#include <yas/text_iarchive.hpp>
#include <yas/json_oarchive.hpp>
#include <yas/json_iarchive.hpp>

#include <yas/mem_streams.hpp>
#include <yas/file_streams.hpp>

#include <yas/detail/tools/hexdumper.hpp>

#include <yas/serializers/std_types_serializers.hpp>

#if defined(YAS_SERIALIZE_BOOST_TYPES)
#include <yas/serializers/boost_types_serializers.hpp>

#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/include/make_vector.hpp>
#include <boost/fusion/sequence/comparison.hpp>
#include <boost/fusion/include/comparison.hpp>
#endif // defined(YAS_SERIALIZE_BOOST_TYPES)

#if defined(YAS_SERIALIZE_QT_TYPES)
#include <yas/serializers/qt_types_serializers.hpp>
#endif

#include "include/array.hpp"
#include "include/auto_array.hpp"
#include "include/bitset.hpp"
#include "include/chrono.hpp"
#include "include/complex.hpp"
#include "include/buffer.hpp"
#include "include/enum.hpp"
#include "include/base_object.hpp"
#include "include/forward_list.hpp"
#include "include/fusion_list.hpp"
#include "include/fusion_map.hpp"
#include "include/fusion_pair.hpp"
#include "include/fusion_set.hpp"
#include "include/fusion_tuple.hpp"
#include "include/fusion_vector.hpp"
#include "include/boost_cont_string.hpp"
#include "include/boost_cont_wstring.hpp"
#include "include/boost_cont_vector.hpp"
#include "include/boost_cont_static_vector.hpp"
#include "include/boost_cont_stable_vector.hpp"
#include "include/boost_cont_list.hpp"
#include "include/boost_cont_slist.hpp"
#include "include/boost_cont_map.hpp"
#include "include/boost_cont_multimap.hpp"
#include "include/boost_cont_set.hpp"
#include "include/boost_cont_multiset.hpp"
#include "include/boost_cont_flat_map.hpp"
#include "include/boost_cont_flat_multimap.hpp"
#include "include/boost_cont_flat_set.hpp"
#include "include/boost_cont_flat_multiset.hpp"
#include "include/boost_cont_deque.hpp"
#include "include/list.hpp"
#include "include/map.hpp"
#include "include/multimap.hpp"
#include "include/multiset.hpp"
#include "include/optional.hpp"
#include "include/pair.hpp"
#include "include/pod.hpp"
#include "include/deque.hpp"
#include "include/set.hpp"
#include "include/string.hpp"
#include "include/tuple.hpp"
#include "include/types.hpp"
#include "include/unordered_map.hpp"
#include "include/unordered_multimap.hpp"
#include "include/unordered_multiset.hpp"
#include "include/unordered_set.hpp"
#include "include/vector.hpp"
#include "include/endian.hpp"
#include "include/version.hpp"
#include "include/wstring.hpp"
#include "include/one_function.hpp"
#include "include/one_method.hpp"
#include "include/split_functions.hpp"
#include "include/split_methods.hpp"
#include "include/serialization_methods.hpp"

/***************************************************************************/

template<bool, typename, typename>
struct concrete_archive_traits;

// mem archives traits
template<typename OA, typename IA>
struct concrete_archive_traits<true, OA, IA> {
	typedef OA oarchive_type;
	typedef IA iarchive_type;

	/** output archive */
	struct oarchive {
		oarchive():oa(0) {}
		~oarchive() { delete oa; }

		oarchive_type* operator->() { return oa; }

		template<typename T>
		oarchive_type& operator& (const T& v) { return (*(oa) & v); }
		oarchive_type& serialize() { return *oa; }
		template<typename Head, typename... Tail>
		oarchive_type& serialize(const Head& head, const Tail&... tail) {
			oa->operator& (head);
			oa->serialize(tail...);

			return *oa;
		}
		template<typename... Ts>
		oarchive_type& operator()(const Ts&... ts) { oa->serialize(ts...); return *oa; }

		static constexpr bool is_little_endian() { return oarchive_type::is_little_endian(); }
		static constexpr bool is_big_endian() { return oarchive_type::is_big_endian(); }
		static constexpr yas::endian_t host_endian() { return oarchive_type::host_endian(); }

		std::uint32_t size() const { return stream.get_intrusive_buffer().size; }
		void dump() {
			const yas::intrusive_buffer buf = stream.get_intrusive_buffer();
			std::cout << yas::hex_dump(buf.data, buf.size) << std::endl;
		}

		bool compare(const void* ptr, std::uint32_t size) const {
			const yas::intrusive_buffer buf = stream.get_intrusive_buffer();
//			std::cout << "ptr=" << std::endl << yas::hex_dump(ptr, size) << std::endl;
//			std::cout << "buf=" << std::endl << yas::hex_dump(buf.data, buf.size) << std::endl;
			return size == buf.size ? (0 == std::memcmp(buf.data, ptr, size)) : false;
		}

		typename oarchive_type::stream_type stream;
		oarchive_type* oa;
	};
	static void ocreate(oarchive& oa, const char* archive_type, const char* io_type) {
		((void)archive_type);
		((void)io_type);
		oa.oa = new oarchive_type(oa.stream);
	}

	/** input archive */
	struct iarchive {
		iarchive():stream(0),ia(0) {}
		~iarchive() { delete ia; delete stream; }

		iarchive_type* operator->() { return ia; }

		template<typename T>
		iarchive_type& operator& (T& v) { return (*(ia) & v); }
		iarchive_type& serialize() { return *ia; }
		template<typename Head, typename... Tail>
		iarchive_type& serialize(Head& head, Tail&... tail) {
			ia->operator&(head);
			ia->serialize(tail...);

			return *ia;
		}
		template<typename... Ts>
		iarchive_type& operator()(Ts&... ts) { return ia->serialize(ts...); return *ia; }

		bool is_little_endian() { return ia->is_little_endian(); }
		bool is_big_endian() { return ia->is_big_endian(); }
		static constexpr yas::endian_t host_endian() { return iarchive_type::host_endian(); }

		typename iarchive_type::stream_type *stream;
		iarchive_type* ia;
	};
	static void icreate(iarchive& ia, oarchive& oa, const char* archive_type, const char* io_type) {
		((void)archive_type);
		((void)io_type);
		ia.stream = new typename iarchive_type::stream_type(oa.stream.get_intrusive_buffer());
		ia.ia = new iarchive_type(*(ia.stream));
	}
};

/***************************************************************************/

std::uint32_t oa_cnt = 0;

// file archives traits
template<typename OA, typename IA>
struct concrete_archive_traits<false, OA, IA> {
	typedef OA oarchive_type;
	typedef IA iarchive_type;

	struct oarchive {
		oarchive():oa(0) {++oa_cnt;}
		~oarchive() {
			delete stream;
			delete oa;
			--oa_cnt;
			std::remove(fname.c_str());
		}

		oarchive_type* operator->() { return oa; }

		template<typename T>
		oarchive_type& operator& (const T& v) { return (*(oa) & v); }
		oarchive_type& serialize() { return *oa; }
		template<typename Head, typename... Tail>
		oarchive_type& serialize(const Head& head, const Tail&... tail) {
			oa->operator&(head);
			oa->serialize(tail...);

			return *oa;
		}
		template<typename... Ts>
		oarchive_type& operator()(const Ts&... ts) { oa->serialize(ts...); return *oa; }

		static constexpr bool is_little_endian() { return oarchive_type::is_little_endian(); }
		static constexpr bool is_big_endian() { return oarchive_type::is_big_endian(); }
		static constexpr yas::endian_t host_endian() { return oarchive_type::host_endian(); }

		std::uint32_t size() {
			stream->flush();
			std::ifstream f(fname);
			f.seekg(0, std::ios::end);
			return f.tellg();
		}
		void dump() {
			std::string str(size(), 0);
			std::ifstream f(fname, std::ios::binary);
			assert(f);
			f.read(&str[0], size());
			std::cout << yas::hex_dump(str.c_str(), size()) << std::endl;
		}

		bool compare(const void* ptr, std::uint32_t size) {
			if ( this->size() != size ) return false;
			std::string str(size, 0);
			std::ifstream f(fname, std::ios::binary);
			assert(f);
			f.read(&str[0], size);
			return memcmp(str.c_str(), ptr, size) == 0;
		}

		std::string fname;
		typename oarchive_type::stream_type *stream;
		oarchive_type* oa;
	};
	static void ocreate(oarchive& oa, const char* archive_type, const char* io_type) {
		((void)io_type);

		oa.fname += archive_type;
		oa.fname += "_";
		oa.fname += std::to_string(oa_cnt);
		oa.fname += ".bin";
		oa.stream = new typename oarchive_type::stream_type(oa.fname.c_str(), yas::file_trunc);
		oa.oa = new oarchive_type(*(oa.stream));
	}

	struct iarchive {
		iarchive():ia(0) {}
		~iarchive() { delete stream; delete ia; }

		iarchive_type* operator->() { return ia; }

		template<typename T>
		iarchive_type& operator& (T& v) { return (*(ia) & v); }
		iarchive_type& serialize() { return *ia; }
		template<typename Head, typename... Tail>
		iarchive_type& serialize(Head& head, Tail&... tail) {
			ia->operator&(head);
			ia->serialize(tail...);

			return *ia;
		}
		template<typename... Ts>
		iarchive_type& operator()(Ts&... ts) { serialize(ts...); return *ia; }

		bool is_little_endian() { return ia->is_little_endian(); }
		bool is_big_endian() { return ia->is_big_endian(); }
		static constexpr yas::endian_t host_endian() { return iarchive_type::host_endian(); }

		std::string fname;
		typename iarchive_type::stream_type *stream;
		iarchive_type* ia;
	};
	static void icreate(iarchive& ia, oarchive& oa, const char* archive_type, const char* io_type) {
		((void)archive_type);
		((void)io_type);
		oa.stream->flush();
		ia.fname = oa.fname;
		ia.stream = new typename iarchive_type::stream_type(oa.fname.c_str());
		ia.ia = new iarchive_type(*(ia.stream));
	}
};

/***************************************************************************/

#define YAS_RUN_TEST(testname, passcnt, failcnt) { \
	static const char * artype = \
		(yas::is_binary_archive<OA>::value ? "binary" \
			: yas::is_text_archive<OA>::value ? "text" \
				: yas::is_json_archive<OA>::value ? "json" \
		: "unknown" \
	); \
	static const char *iotype = (mem ? "mem" : "file"); \
	std::fprintf( \
		 stdout \
		,"%-6s %-4s: %-24s -> %s\n" \
		,artype /* 1 */ \
		,iotype /* 2 */ \
		,#testname /* 3 */ \
		,(testname##_test<concrete_archive_traits<mem, OA, IA>>(artype, iotype) \
			?(++passcnt,"passed") \
			:(++failcnt,"failed!") \
		) /* 4 */ \
	); \
}

template<bool mem, typename OA, typename IA>
void tests(std::uint32_t& p, std::uint32_t& e) {
	YAS_RUN_TEST(endian						, p, e);
	YAS_RUN_TEST(version					, p, e);
	YAS_RUN_TEST(pod						, p, e);
	YAS_RUN_TEST(enum						, p, e);
	YAS_RUN_TEST(base_object				, p, e);
	YAS_RUN_TEST(auto_array					, p, e);
	YAS_RUN_TEST(array						, p, e);
	YAS_RUN_TEST(bitset						, p, e);
	YAS_RUN_TEST(buffer						, p, e);
	YAS_RUN_TEST(chrono					, p, e)
	YAS_RUN_TEST(complex					, p, e);
	YAS_RUN_TEST(string						, p, e);
	YAS_RUN_TEST(wstring					, p, e);
	YAS_RUN_TEST(pair						, p, e);
	YAS_RUN_TEST(tuple						, p, e);
	YAS_RUN_TEST(vector						, p, e);
	YAS_RUN_TEST(list						, p, e);
	YAS_RUN_TEST(forward_list				, p, e);
	YAS_RUN_TEST(map						, p, e);
	YAS_RUN_TEST(deque						, p, e);
	YAS_RUN_TEST(set						, p, e);
	YAS_RUN_TEST(multimap					, p, e);
	YAS_RUN_TEST(multiset					, p, e);
	YAS_RUN_TEST(unordered_map				, p, e);
	YAS_RUN_TEST(unordered_set				, p, e);
	YAS_RUN_TEST(unordered_multimap			, p, e);
	YAS_RUN_TEST(unordered_multiset			, p, e);
	YAS_RUN_TEST(optional				, p, e);
#if defined(YAS_HAS_BOOST_FUSION)
	YAS_RUN_TEST(fusion_pair				, p, e);
	YAS_RUN_TEST(fusion_tuple				, p, e);
	YAS_RUN_TEST(fusion_vector				, p, e);
	YAS_RUN_TEST(fusion_list				, p, e);
	YAS_RUN_TEST(fusion_set					, p, e);
	YAS_RUN_TEST(fusion_map					, p, e);
#endif // YAS_HAS_BOOST_FUSION
#if defined(YAS_SERIALIZE_BOOST_TYPES)
	YAS_RUN_TEST(boost_cont_string			, p, e);
	YAS_RUN_TEST(boost_cont_wstring			, p, e);
	YAS_RUN_TEST(boost_cont_vector			, p, e);
	YAS_RUN_TEST(boost_cont_static_vector	, p, e);
	YAS_RUN_TEST(boost_cont_stable_vector	, p, e);
	YAS_RUN_TEST(boost_cont_list			, p, e);
	YAS_RUN_TEST(boost_cont_slist			, p, e);
	YAS_RUN_TEST(boost_cont_map				, p, e);
	YAS_RUN_TEST(boost_cont_multimap		, p, e);
	YAS_RUN_TEST(boost_cont_set				, p, e);
	YAS_RUN_TEST(boost_cont_multiset		, p, e);
	YAS_RUN_TEST(boost_cont_flat_map		, p, e);
	YAS_RUN_TEST(boost_cont_flat_multimap	, p, e);
	YAS_RUN_TEST(boost_cont_flat_set		, p, e);
	YAS_RUN_TEST(boost_cont_flat_multiset	, p, e);
	YAS_RUN_TEST(boost_cont_deque			, p, e);
#endif // YAS_SERIALIZE_BOOST_TYPES
	YAS_RUN_TEST(one_function				, p, e);
	YAS_RUN_TEST(split_functions			, p, e);
	YAS_RUN_TEST(one_method					, p, e);
	YAS_RUN_TEST(split_methods				, p, e);
	YAS_RUN_TEST(serialization_methods	, p, e);
}

/***************************************************************************/

int main() {
	setvbuf(stdout, 0, _IONBF, 0);

	types_test();

	std::uint32_t passed = 0;
	std::uint32_t failed = 0;

	try {
		tests<true , yas::binary_oarchive<yas::mem_ostream>, yas::binary_iarchive<yas::mem_istream>>(passed, failed);
		tests<false, yas::binary_oarchive<yas::file_ostream>, yas::binary_iarchive<yas::file_istream>>(passed, failed);
		tests<true , yas::text_oarchive<yas::mem_ostream>, yas::text_iarchive<yas::mem_istream>>(passed, failed);
		tests<false, yas::text_oarchive<yas::file_ostream>, yas::text_iarchive<yas::file_istream>>(passed, failed);
	} catch (const std::exception &ex) {
		std::cout << "[exception]: " << ex.what() << std::endl;
	}

	std::cout << std::endl
	<< "/***************************************************/" << std::endl
	<< "> platform bits  : " << (YAS_PLATFORM_BITS()) << std::endl
	<< "> platform endian: " << (YAS_LITTLE_ENDIAN() ? "little" : "big") << std::endl
	<< "> passed tests   : " << passed << std::endl
	<< "> failed tests   : " << failed << std::endl
	<< "/***************************************************/" << std::endl;

	return failed;
}

/***************************************************************************/
