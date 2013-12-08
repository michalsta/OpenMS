// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2013.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Stephan Aiche $
// $Authors: Stephan Aiche $
// --------------------------------------------------------------------------

#ifndef OPENMS_DATASTRUCTURES_LISTUTILS_H
#define OPENMS_DATASTRUCTURES_LISTUTILS_H

#include <vector>
#include <algorithm>
#include <ostream>
#include <iterator>

#include <OpenMS/DATASTRUCTURES/String.h>
#include <OpenMS/CONCEPT/Exception.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace OpenMS
{


  /**
    @brief Vector of signed integers.

    @ingroup Datastructures
  */
  typedef std::vector<Int> IntList;

  /**
   @brief Vector of double precision real types.

   @ingroup Datastructures
   */
  typedef std::vector<DoubleReal> DoubleList;


  /**
   @brief Vector of String.

   @ingroup Datastructures
   */
  typedef std::vector<String> StringList;


  /// @cond INTERNAL
  /**
    @brief Internal wrapper for the create functions that allows specialisation by the return type.

    Explicit specialisation for String exists, all other types are handled by boost::lexical_cast.
  */
  namespace Internal
  {
    namespace _convert_wrapper
    {
      /// generic create using boost::lexical_cast
      template <typename T>
      inline std::vector<T> create_(const std::vector<String>& s, const T*)
      {
        std::vector<T> c;
        c.reserve(s.size());
        for (std::vector<String>::const_iterator it = s.begin(); it != s.end(); ++it)
        {
          try
          {
            c.push_back(boost::lexical_cast<T>(boost::trim_copy(*it)));
          }
          catch (boost::bad_lexical_cast&)
          {
            throw Exception::ConversionError(__FILE__, __LINE__, __PRETTY_FUNCTION__, String("Could not convert string '") + *it + "'");
          }
        }

        return c;
      }

      /// create specialisation for String since we do not need to cast here
      template <>
      inline std::vector<String> create_(const std::vector<String>& s, const String*)
      {
        return s;
      }

    }
  }
  /// @endcond

  /**
    @brief Collection of utility functions for management of vectors.

    @ingroup Datastructures
  */
  class OPENMS_DLLAPI ListUtils
  {
private:
    /**
      @brief Predicate that to check double equality with a given tolerance.
    */
    struct DoubleTolerancePredicate_
    {
      DoubleTolerancePredicate_(const DoubleReal& target, const DoubleReal& tolerance) :
        tolerance_(tolerance),
        target_(target)
      {}

      /**
        @brief Returns true if \| @p value - @p target \| \< @p tolerance.

        @param value The value to test.
        @return true if \| @p value - @p target \| \< @p tolerance, false otherwise.
      */
      inline bool operator()(const DoubleReal& value)
      {
        return std::fabs(value - target_) < tolerance_;
      }

private:
      /// The allowed tolerance.
      DoubleReal tolerance_;
      /// The target value that should be found.
      DoubleReal target_;
    };

public:
    /**
      @brief Returns a list that is created by splitting the given comma-separated string.
      @note If converted to vector<String> the strings are not trimmed.
      @note The values get converted by boost::lexical_cast so a valid conversion from String to T needs to be available.

      @param str The string that should be split and converted to a list.
      @return A vector containing the elements of the string converted into type T.
    */
    template <typename T>
    static std::vector<T> create(const String& str, const char splitter = ',')
    {
      // temporary storage for the individual elements of the string
      std::vector<String> temp_string_vec;
      str.split(splitter, temp_string_vec);
      return create<T>(temp_string_vec);
    }

    /**
      @brief Converts a vector of strings to a vector of the target type T.
      @note The strings are not trimmed.
      @note The values get converted by boost::lexical_cast so a valid conversion from String to T needs to be available.

      @param s The vector of strings that should be converted.
      @return A vector containing the elements of input vector converted into type T.
    */
    template <typename T>
    static std::vector<T> create(const std::vector<String>& s)
    {
      return Internal::_convert_wrapper::create_(s, (T*) NULL);
    }

    /**
      @brief Checks whether the element @p elem is contained in the given container.

      @param container The container to check.
      @param elem The element to check whether it is in the container or not.

      @return True if @p elem is contained in @p container, false otherwise.
    */
    template <typename T, typename E>
    static bool contains(const std::vector<T>& container, const E& elem)
    {
      return find(container.begin(), container.end(), elem) != container.end();
    }

    /**
      @brief Checks whether the element @p elem is contained in the given container of floating point numbers.

      @param container The container of doubles to check.
      @param elem The element to check whether it is in the container or not.
      @param tolerance The allowed tolerance for the double.

      @return True if @p elem is contained in @p container, false otherwise.
    */
    static bool contains(const std::vector<DoubleReal>& container, const DoubleReal& elem, DoubleReal tolerance = 0.00001)
    {
      return find_if(container.begin(), container.end(), DoubleTolerancePredicate_(elem, tolerance)) != container.end();
    }

    /**
      @brief Concatenates all elements of the @p container and puts the @p glue string between elements.

      @param container The container to concatenate.
      @param glue The string to add in between elements.
    */
    template <typename T>
    static String concatenate(const std::vector<T>& container, const String& glue = "")
    {
      // handle empty containers
      if (container.empty()) return "";

      typename std::vector<T>::const_iterator it = container.begin();
      String ret = String(*it);
      // we have handled the first element
      ++it;
      // add the rest
      for (; it != container.end(); ++it)
      {
        ret += (glue + String(*it));
      }

      return ret;
    }

  };

  /**
    @brief Output stream operator for std::vectors.

    @param os The target stream.
    @param v The vector to write to stream.
  */
  template <typename T>
  inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
  {
    // handle precision settings
    const std::streamsize prec_save = os.precision();
    os << std::setprecision(writtenDigits(T()));

    os << "[";

    if (!v.empty())
    {
      std::copy(v.begin(), v.end() - 1, std::ostream_iterator<T>(os, ", "));
      os << v.back();
    }

    os << "]";
    // set precision settings back to original values
    os << std::setprecision(prec_save);
    return os;
  }

  /// Operator for appending entries with less code
  template <typename TString>
  inline std::vector<String>& operator<<(std::vector<String>& sl, const TString& string)
  {
    sl.push_back(string);
    return sl;
  }

} // namespace OpenMS

#endif // OPENMS_DATASTRUCTURES_LISTUTILS_H