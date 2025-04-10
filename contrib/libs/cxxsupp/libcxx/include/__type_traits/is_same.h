//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___TYPE_TRAITS_IS_SAME_H
#define _LIBCPP___TYPE_TRAITS_IS_SAME_H

#include <__config>
#include <__type_traits/integral_constant.h>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if __has_keyword(__is_same) && !defined(__CUDACC__)

template <class _Tp, class _Up>
struct _LIBCPP_TEMPLATE_VIS is_same : _BoolConstant<__is_same(_Tp, _Up)> {};

#  if _LIBCPP_STD_VER >= 17
template <class _Tp, class _Up>
inline constexpr bool is_same_v = __is_same(_Tp, _Up);
#  endif

#else

template <class _Tp, class _Up>
struct _LIBCPP_TEMPLATE_VIS is_same : public false_type {};
template <class _Tp>
struct _LIBCPP_TEMPLATE_VIS is_same<_Tp, _Tp> : public true_type {};

#  if _LIBCPP_STD_VER > 14
template <class _Tp, class _Up>
inline constexpr bool is_same_v = is_same<_Tp, _Up>::value;
#  endif

#endif // __is_same
// _IsSame<T,U> has the same effect as is_same<T,U> but instantiates fewer types:
// is_same<A,B> and is_same<C,D> are guaranteed to be different types, but
// _IsSame<A,B> and _IsSame<C,D> are the same type (namely, false_type).
// Neither GCC nor Clang can mangle the __is_same builtin, so _IsSame
// mustn't be directly used anywhere that contributes to name-mangling
// (such as in a dependent return type).

template <class _Tp, class _Up>
using _IsSame = _BoolConstant<
#if defined(__clang__) && !defined(__CUDACC__)
    __is_same(_Tp, _Up)
#else
    is_same<_Tp, _Up>::value
#endif
    >;

template <class _Tp, class _Up>
using _IsNotSame = _BoolConstant<
#if defined(__clang__) && !defined(__CUDACC__)
    !__is_same(_Tp, _Up)
#else
    !is_same<_Tp, _Up>::value
#endif
    >;

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___TYPE_TRAITS_IS_SAME_H
