/*
Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/
#include "value.h"
#include <iomanip>
#include "binlog_event.h"

using namespace mysql;
using namespace mysql::system;
namespace mysql {

int calc_field_size(unsigned char column_type, const unsigned char *field_ptr, uint32_t metadata)
{
  uint32_t length;

  switch (column_type) {
  case mysql::system::MYSQL_TYPE_VAR_STRING:
    /* This type is hijacked for result set types. */
    length= metadata;
    break;
  case mysql::system::MYSQL_TYPE_NEWDECIMAL:
    //length= my_decimal_get_binary_size(metadata_ptr[col] >> 8,
    //                                   metadata_ptr[col] & 0xff);
    length= 0;
    break;
  case mysql::system::MYSQL_TYPE_DECIMAL:
  case mysql::system::MYSQL_TYPE_FLOAT:
  case mysql::system::MYSQL_TYPE_DOUBLE:
    length= metadata;
    break;
  /*
    The cases for SET and ENUM are include for completeness, however
    both are mapped to type MYSQL_TYPE_STRING and their real types
    are encoded in the field metadata.
  */
  case mysql::system::MYSQL_TYPE_SET:
  case mysql::system::MYSQL_TYPE_ENUM:
  case mysql::system::MYSQL_TYPE_STRING:
  {
    unsigned char type= metadata >> 8U;
    if ((type == mysql::system::MYSQL_TYPE_SET) || (type == mysql::system::MYSQL_TYPE_ENUM))
      length= metadata & 0x00ff;
    else
    {
      /*
        We are reading the actual size from the master_data record
        because this field has the actual lengh stored in the first
        byte.
      */
      length= (unsigned int) *field_ptr+1;
      //DBUG_ASSERT(length != 0);
    }
    break;
  }
  case mysql::system::MYSQL_TYPE_YEAR:
  case mysql::system::MYSQL_TYPE_TINY:
    length= 1;
    break;
  case mysql::system::MYSQL_TYPE_SHORT:
    length= 2;
    break;
  case mysql::system::MYSQL_TYPE_INT24:
    length= 3;
    break;
  case mysql::system::MYSQL_TYPE_LONG:
    length= 4;
    break;
  case MYSQL_TYPE_LONGLONG:
    length= 8;
    break;
  case mysql::system::MYSQL_TYPE_NULL:
    length= 0;
    break;
  case mysql::system::MYSQL_TYPE_NEWDATE:
    length= 3;
    break;
  case mysql::system::MYSQL_TYPE_DATE:
  case mysql::system::MYSQL_TYPE_TIME:
    length= 3;
    break;
  case mysql::system::MYSQL_TYPE_TIMESTAMP:
    length= 4;
    break;
  case mysql::system::MYSQL_TYPE_DATETIME:
    length= 8;
    break;
  case mysql::system::MYSQL_TYPE_BIT:
  {
    /*
      Decode the size of the bit field from the master.
        from_len is the length in bytes from the master
        from_bit_len is the number of extra bits stored in the master record
      If from_bit_len is not 0, add 1 to the length to account for accurate
      number of bytes needed.
    */
	uint32_t from_len= (metadata >> 8U) & 0x00ff;
	uint32_t from_bit_len= metadata & 0x00ff;
    //DBUG_ASSERT(from_bit_len <= 7);
    length= from_len + ((from_bit_len > 0) ? 1 : 0);
    break;
  }
  case mysql::system::MYSQL_TYPE_VARCHAR:
  {
    length= metadata > 255 ? 2 : 1;

    length+= length == 1 ? (uint32_t) *field_ptr : *((uint16_t *)field_ptr);

    break;
  }
  case mysql::system::MYSQL_TYPE_TINY_BLOB:
  case mysql::system::MYSQL_TYPE_MEDIUM_BLOB:
  case mysql::system::MYSQL_TYPE_LONG_BLOB:
  case mysql::system::MYSQL_TYPE_BLOB:
  case mysql::system::MYSQL_TYPE_GEOMETRY:
  {
     switch (metadata)
    {
      case 1:
        length= 1+ (uint32_t) field_ptr[0];
        break;
      case 2:
        length= 2+ (uint32_t) (*(uint16_t *)(field_ptr) & 0xFFFF);
        break;
      case 3:
        // TODO make platform indep.
        length= 3+ (uint32_t) (long) (*((uint32_t *) (field_ptr)) & 0xFFFFFF);
        break;
      case 4:
        // TODO make platform indep.
        length= 4+ (uint32_t) (long) *((uint32_t *) (field_ptr));
        break;
      default:
        length= 0;
        break;
    }
    break;
  }
  default:
    length= ~(uint32_t) 0;
  }
  return length;
}

/*
Value::Value(Value &val)
{
  m_size= val.length();
  m_storage= val.storage();
  m_type= val.type();
  m_metadata= val.metadata();
  m_is_null= val.is_null();
}
*/

Value::Value(const Value& val)
{
  m_size= val.m_size;
  m_storage= val.m_storage;
  m_type= val.m_type;
  m_metadata= val.m_metadata;
  m_is_null= val.m_is_null;
}

Value &Value::operator=(const Value &val)
{
  m_size= val.m_size;
  m_storage= val.m_storage;
  m_type= val.m_type;
  m_metadata= val.m_metadata;
  m_is_null= val.m_is_null;
  return *this;
}

bool Value::operator==(const Value &val) const
{
  return (m_size == val.m_size) &&
         (m_storage == val.m_storage) &&
         (m_type == val.m_type) &&
         (m_metadata == val.m_metadata);
}

bool Value::operator!=(const Value &val) const
{
  return !operator==(val);
}

char *Value::as_c_str(unsigned long &size) const
{
  if (m_is_null || m_size == 0)
  {
    size = 0;
    return NULL;
  }
  /*
   Length encoded; First byte is length of string.
  */

  int metadata_length= m_metadata > 255 ? 2 : 1;

  // std::cout << "m_size " << metadata_length << std::endl;

  /*
   Size is length of the character string; not of the entire storage
  */
  size= m_size - metadata_length;

  return const_cast<char *>(m_storage + metadata_length);
}

unsigned char *Value::as_blob(unsigned long &size) const
{
  if (m_is_null || m_size == 0)
  {
    size= 0;
    return 0;
  }

  /*
   Size was calculated during construction of the object and only inludes the
   size of the blob data, not the metadata part which also is stored in the
   storage. For blobs this part can be between 1-4 bytes long.
  */
  size= m_size - m_metadata;

  /*
   Adjust the storage pointer with the size of the metadata.
  */
  return (unsigned char *)(m_storage + m_metadata);
}

int32_t Value::as_int32() const
{
  if (m_is_null)
  {
    return 0;
  }
  uint32_t to_int;
  Protocol_chunk<uint32_t> prot_integer(to_int);

  buffer_source buff(m_storage, m_size);
  buff >> prot_integer;
  return to_int;
}

int8_t Value::as_int8() const
{
  if (m_is_null)
  {
    return 0;
  }
  int8_t to_int;
  Protocol_chunk<int8_t> prot_integer(to_int);

  buffer_source buff(m_storage, m_size);
  buff >> prot_integer;
  return to_int;
}

int16_t Value::as_int16() const
{
  if (m_is_null)
  {
    return 0;
  }
  int16_t to_int;
  Protocol_chunk<int16_t> prot_integer(to_int);

  buffer_source buff(m_storage, m_size);
  buff >> prot_integer;
  return to_int;
}

int64_t Value::as_int64() const
{
  if (m_is_null)
  {
    return 0;
  }
  int64_t to_int;
  Protocol_chunk<int64_t> prot_integer(to_int);

  buffer_source buff(m_storage, m_size);
  buff >> prot_integer;
  return to_int;
}

float Value::as_float() const
{
  // TODO
  return *((const float *)storage());
}

double Value::as_double() const
{
  // TODO
  return *((const double *)storage());
}

void Converter::to(std::string &str, const Value &val) const
{
  if (val.is_null())
  {
    str = "NULL";
    return;
  }

  std::ostringstream os;
  switch(val.type())
  {
    case MYSQL_TYPE_DECIMAL:
      str = "not implemented";
      return;
    case MYSQL_TYPE_TINY:
      os << static_cast<int>(val.as_int8());
      break;
    case MYSQL_TYPE_SHORT:
      os << val.as_int16();
      break;
    case MYSQL_TYPE_LONG:
      os << val.as_int32();
      break;
    case MYSQL_TYPE_FLOAT:
      os << val.as_float();
      break;
    case MYSQL_TYPE_DOUBLE:
      os << val.as_double();
      break;
    case MYSQL_TYPE_NULL:
      str = "not implemented";
      return;
    case MYSQL_TYPE_TIMESTAMP:
      os << (uint32_t)val.as_int32();
      break;
    case MYSQL_TYPE_LONGLONG:
      os << val.as_int64();
      break;
    case MYSQL_TYPE_INT24:
      str = "not implemented";
      return;
    case MYSQL_TYPE_DATE:
      str = "not implemented";
      return;
    case MYSQL_TYPE_DATETIME:
      {
      uint64_t timestamp= val.as_int64();
      unsigned long d= timestamp / 1000000;
      unsigned long t= timestamp % 1000000;

      os << std::setfill('0') << std::setw(4) << d / 10000
         << std::setw(1) << '-'
         << std::setw(2) << (d % 10000) / 100
         << std::setw(1) << '-'
         << std::setw(2) << d % 100
         << std::setw(1) << ' '
         << std::setw(2) << t / 10000
         << std::setw(1) << ':'
         << std::setw(2) << (t % 10000) / 100
         << std::setw(1) << ':'
         << std::setw(2) << t % 100;
      }
      break;
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
      str = "not implemented";
      return;
    case MYSQL_TYPE_VARCHAR:
    {
      unsigned long size;

      char *ptr= val.as_c_str(size);

      str.assign(ptr, size);
    }
      return;
    case MYSQL_TYPE_VAR_STRING:
    {
      str.assign(val.storage(), val.length());
    }
      return;
    case MYSQL_TYPE_STRING:
    {
      unsigned long size;
      char *ptr = val.as_c_str(size);
      str.assign(ptr, size);
    }
      return;
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
      str = "not implemented";
      return ;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    {
      unsigned long size;
      unsigned char *ptr= val.as_blob(size);
      str.assign((const char *)ptr, size);
    }
      return;
    case MYSQL_TYPE_GEOMETRY:
    default:
      str = "not implemented";
      return;
  }
  str = os.str();
}

void Converter::to(float &out, const Value &val) const
{
  switch(val.type())
  {
  case MYSQL_TYPE_FLOAT:
    out= val.as_float();
    break;
  default:
    out= 0;
  }
}


void Converter::to(long long &out, const Value &val) const
{
	switch(val.type())
	  {
	    case MYSQL_TYPE_DECIMAL:
	      // TODO
	      out= 0;
	      break;
	    case MYSQL_TYPE_TINY:
	      out= val.as_int8();
	      break;
	    case MYSQL_TYPE_SHORT:
	      out= val.as_int16();
	      break;;
	    case MYSQL_TYPE_LONG:
	      out= (long long)val.as_int32();
	      break;
	    case MYSQL_TYPE_FLOAT:
	      out= 0;
	      break;
	    case MYSQL_TYPE_DOUBLE:
	      out= (long long)val.as_double();
	    case MYSQL_TYPE_NULL:
	      out= 0;
	      break;
	    case MYSQL_TYPE_TIMESTAMP:
	      out=(uint32_t)val.as_int32();
	      break;

	    case MYSQL_TYPE_LONGLONG:
	      out= (long long)val.as_int64();
	      break;
	    case MYSQL_TYPE_INT24:
	      out= 0;
	      break;
	    case MYSQL_TYPE_DATE:
	      out= 0;
	      break;
	    case MYSQL_TYPE_TIME:
	      out= 0;
	      break;
	    case MYSQL_TYPE_DATETIME:
	      out= (long long)val.as_int64();
	      break;
	    case MYSQL_TYPE_YEAR:
	      out= 0;
	      break;
	    case MYSQL_TYPE_NEWDATE:
	      out= 0;
	      break;
	    case MYSQL_TYPE_VARCHAR:
	      out= 0;
	      break;
	    case MYSQL_TYPE_BIT:
	      out= 0;
	      break;
	    case MYSQL_TYPE_NEWDECIMAL:
	      out= 0;
	      break;
	    case MYSQL_TYPE_ENUM:
	      out= 0;
	      break;
	    case MYSQL_TYPE_SET:
	      out= 0;
	      break;
	    case MYSQL_TYPE_TINY_BLOB:
	    case MYSQL_TYPE_MEDIUM_BLOB:
	    case MYSQL_TYPE_LONG_BLOB:
	    case MYSQL_TYPE_BLOB:
	      out= 0;
	      break;
	    case MYSQL_TYPE_VAR_STRING:
	    {
          std::string str;
          str.append(val.storage(), val.length());
          std::istringstream is(str);
          is >> out;
	    }
	      break;
	    case MYSQL_TYPE_STRING:
	      out= 0;
	      break;
	    case MYSQL_TYPE_GEOMETRY:
	      out= 0;
	      break;
	    default:
	      out= 0;
	      break;
	  }
}

void Converter::to(long &out, const Value &val) const
{
  switch(val.type())
  {
    case MYSQL_TYPE_DECIMAL:
      // TODO
      out= 0;
      break;
    case MYSQL_TYPE_TINY:
      out= val.as_int8();
      break;
    case MYSQL_TYPE_SHORT:
      out= val.as_int16();
      break;;
    case MYSQL_TYPE_LONG:
      out= (long)val.as_int32();
      break;
    case MYSQL_TYPE_FLOAT:
      out= 0;
      break;
    case MYSQL_TYPE_DOUBLE:
      out= (long)val.as_double();
    case MYSQL_TYPE_NULL:
      out= 0;
      break;
    case MYSQL_TYPE_TIMESTAMP:
      out=(uint32_t)val.as_int32();
      break;

    case MYSQL_TYPE_LONGLONG:
      out= (long)val.as_int64();
      break;
    case MYSQL_TYPE_INT24:
      out= 0;
      break;
    case MYSQL_TYPE_DATE:
      out= 0;
      break;
    case MYSQL_TYPE_TIME:
      out= 0;
      break;
    case MYSQL_TYPE_DATETIME:
      out= (long)val.as_int64();
      break;
    case MYSQL_TYPE_YEAR:
      out= 0;
      break;
    case MYSQL_TYPE_NEWDATE:
      out= 0;
      break;
    case MYSQL_TYPE_VARCHAR:
      out= 0;
      break;
    case MYSQL_TYPE_BIT:
      out= 0;
      break;
    case MYSQL_TYPE_NEWDECIMAL:
      out= 0;
      break;
    case MYSQL_TYPE_ENUM:
      out= 0;
      break;
    case MYSQL_TYPE_SET:
      out= 0;
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      out= 0;
      break;
    case MYSQL_TYPE_VAR_STRING:
    {
      std::string str;
      str.append(val.storage(), val.length());
      std::istringstream is(str);
      is >> out;
    }
      break;
    case MYSQL_TYPE_STRING:
      out= 0;
      break;
    case MYSQL_TYPE_GEOMETRY:
      out= 0;
      break;
    default:
      out= 0;
      break;
  }
}


} // end namespace mysql
