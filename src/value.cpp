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
#include <iomanip>

#include "value.h"
#include "binlog_event.h"
#include "decimal.h"

using namespace mysql;
using namespace mysql::system;
namespace mysql {

// calculate mysql NEW DECIMAL digits's size in bytes
static uint8_t calc_digits_size(uint8_t digits)
{
    uint8_t size = 0;

    while (digits > 9) {
        digits -= 9;
        size   += 4;
    }
    if (digits >=7) {
        size   += 4;
    } else if (digits >= 5 && digits <= 6) {
        size   += 3;
    } else if (digits >= 3 && digits <= 4) {
        size   += 2;
    } else if (digits >= 1 && digits <= 2) {
        size   += 1;
    }
    return size;
}

uint8_t calc_newdecimal_size(uint8_t m, uint8_t d)
{
  // unsigned char digits_per_bytes[] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 4 };  
  // uint8_t i_digits = m - d;
  // uint8_t f_digits = d;
  // uint8_t decimal_full_blocks = i_digits / 9;
  // uint8_t decimal_last_block_digits = i_digits % 9;
  // uint8_t scale_full_blocks = f_digits / 9;
  // uint8_t scale_last_block_digits = f_digits % 9;
  // uint8_t size = 0;

  // size += decimal_full_blocks * digits_per_bytes[9] + digits_per_bytes[decimal_last_block_digits];
  // size += scale_full_blocks   * digits_per_bytes[9] + digits_per_bytes[scale_last_block_digits];

  // (m - d) is left digits

  return calc_digits_size(m-d) + calc_digits_size(d);
  
}

int calc_field_size(enum mysql::system::enum_field_types column_type, const unsigned char *field_ptr, uint16_t metadata)
{
  uint32_t length;

  switch (column_type) {
  case mysql::system::MYSQL_TYPE_VAR_STRING:
    /* This type is hijacked for result set types. */
    length = metadata;
    break;
  case mysql::system::MYSQL_TYPE_NEWDECIMAL:
    // higher byte is number of digits to the right of the decimal point
    // lower byte is the maximum number of digits
    length = calc_newdecimal_size(metadata & 0xff, metadata >> 8);
    // length = 0;
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

    THIS WILL NEVER BE EXECUTED.
  */
  case mysql::system::MYSQL_TYPE_SET:
  case mysql::system::MYSQL_TYPE_ENUM:
  {
    length = (metadata >> 8U);
    break;
  }
  case mysql::system::MYSQL_TYPE_STRING:
  {
    uint32_t maxlen = 0;
    uint8_t  lower  = metadata & 0xFF;
    if (lower == mysql::system::MYSQL_TYPE_SET || lower == mysql::system::MYSQL_TYPE_ENUM) {
        length = (metadata >> 8U);
        break;
    }
    uint8_t  higher = metadata >> 8U;
    if ((lower & 0x30) != 0x30) {
      maxlen = (((lower & 0x30) ^ 0x30) << 4) | higher;
    } else {
      maxlen = higher;
    }
    length = maxlen >= 256 ? (uint16_t)(*field_ptr) + 2 : (uint8_t) (*field_ptr) + 1;
    // std::cout << "Len: " << length << std::endl;
    break;
  }
  case mysql::system::MYSQL_TYPE_YEAR:
  case mysql::system::MYSQL_TYPE_TINY:
    length = 1;
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
    uint8_t from_len     = metadata >> 8U;
    uint8_t from_bit_len = metadata & 0xff;
    length = from_len + ((from_bit_len > 0) ? 1 : 0);
    break;
  }
  case mysql::system::MYSQL_TYPE_VARCHAR:
  {
    length  = metadata > 255 ? 2 : 1;

    length += length == 1 ? (uint32_t) *field_ptr : *((uint16_t *)field_ptr);

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
  if (m_is_null || m_size == 0) {
    size = 0;
    return NULL;
  }

  if (m_size == 1) {
    size = 0;
    return const_cast<char *>(m_storage);
  }
  /*
   Length encoded; First byte is length of string.
  */

  uint32_t maxlen = 0;
  
  if(m_type == mysql::system::MYSQL_TYPE_STRING) {
    uint8_t  lower  = m_metadata & 0xFF;
    uint8_t  higher = m_metadata >> 8U;
    if ((lower & 0x30) != 0x30) {
      maxlen = (((lower & 0x30) ^ 0x30) << 4) | higher;
    } else {
      maxlen = higher;
    }
  } else {
    maxlen = m_metadata;
  }
  
  int metadata_length = maxlen > 255 ? 2 : 1;

  /*
   Size is length of the character string; not of the entire storage
  */
  size = m_size - metadata_length;

  // std::cout << "Len: " << m_size << " Metadata: " << m_metadata << " Size: " << size << " Max Len: " << maxlen << std::endl;
  
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
  int32_t to_int;
  Protocol_chunk<int32_t> prot_integer(to_int);

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
  if (!val.storage()) {
    str = "NULL";
    return;
  }

  std::ostringstream os;

  switch(val.type())
  {
    case MYSQL_TYPE_DECIMAL:
    {
      str = "not implemented";
      str = os.str();
      break;
    }
    case MYSQL_TYPE_TINY:
    {
      os << static_cast<int>(val.as_int8());
      str = os.str();
      break;
    }
    case MYSQL_TYPE_SHORT:
    {
      os << val.as_int16();
      str = os.str();
      break;
    }
    case MYSQL_TYPE_LONG:
    {
      os << val.as_int32();
      str = os.str();
      break;
    }
    case MYSQL_TYPE_FLOAT:
    {
      os << val.as_float();
      str = os.str();
      break;
    }
    case MYSQL_TYPE_DOUBLE:
    {
      os << val.as_double();
      str = os.str();
      break;
    }
    case MYSQL_TYPE_NULL:
    {
      str = "not implemented";
      str = os.str();
      break;
    }
    case MYSQL_TYPE_TIMESTAMP:
    {
      os << (uint32_t)val.as_int32();
      str = os.str();
      break;
    }
    case MYSQL_TYPE_LONGLONG:
    {
      os << val.as_int64();
      str = os.str();
      break;
    }
    case MYSQL_TYPE_INT24:
    {
      str = "not implemented";
      str = os.str();
      break;
    }
    case MYSQL_TYPE_DATE:
    {
      const char *storage = val.storage();
      uint32_t date       = (storage[0] & 0xff) + ((storage[1] & 0xff) << 8) + ((storage[2] & 0xff) << 16);
      // actually date occupies 3 bytes, so below will work
      uint32_t year       = date >> 9;
      date -= (year << 9);
      uint32_t month      = date >> 5;
      date -= (month << 5);
      uint32_t day        = date;
      os << std::setfill('0') << std::setw(4) << year
         << std::setw(1) << '-'
         << std::setw(2) << month
         << std::setw(1) << '-'
         << std::setw(2) << day;
      str = os.str();
      break;
    }
    case MYSQL_TYPE_DATETIME:
    {
      uint64_t timestamp = val.as_int64();
      unsigned long d = timestamp / 1000000;
      unsigned long t = timestamp % 1000000;

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
      str = os.str();
      break;
    }
    case MYSQL_TYPE_TIME:
      {
      const char *storage = val.storage();
      uint32_t time       = (storage[0] & 0xff) + ((storage[1] & 0xff) << 8) + ((storage[2] & 0xff) << 16);
      uint32_t sec        = time % 100;
      time -= sec;
      uint32_t min        = (time % 10000) / 100;
      uint32_t hour       = (time - min)   / 10000;
      os << std::setfill('0') << std::setw(2) << hour
         << std::setw(1) << ':'
         << std::setw(2) << min
         << std::setw(1) << ':'
         << std::setw(2) << sec;
      str = os.str();
      break;
    }
    case MYSQL_TYPE_YEAR:
    {
      const char *storage = val.storage();
      uint32_t year       = (storage[0] & 0xff);
      if (year > 0) {
          year += 1900;
      }
      os << std::setfill('0') << std::setw(4) << year;
      str = os.str();
      break;
    }
    case MYSQL_TYPE_NEWDATE:
    {
      str = "not implemented";
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    {
      unsigned long size;
      char *ptr= val.as_c_str(size);
      str.append(ptr, size);
      break;
    }
    case MYSQL_TYPE_VAR_STRING:
    {
      str.append(val.storage(), val.length());
      break;
    }
    case MYSQL_TYPE_STRING:
    {
      unsigned long size;
      char *ptr = val.as_c_str(size);
      str.append(ptr, size);
      break;
    }
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_NEWDECIMAL:
    {
        char buffer[100], result[100];
        int  len = 100;
        decimal_t decimal;
        decimal.buf = (decimal_digit_t *)buffer;
        decimal.len = sizeof(buffer)/sizeof(decimal_digit_t);
        bin2decimal((const u_char *)val.storage(), &decimal, val.metadata() & 0xff, val.metadata() >> 8);
        decimal2string(&decimal, result, &len, 0, 0, 0);
        str.append(result, len);
        break;
    }
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    {
      os << (val.length() == 1 ? static_cast<int>(val.as_int8()) : val.as_int16());
      str = os.str();
      break;
    }
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    {
      unsigned long size;
      unsigned char *ptr= val.as_blob(size);
      str.append((const char *)ptr, size);
      break;
    }
    case MYSQL_TYPE_GEOMETRY:
    default:
      str = "not implemented";
      break;
  }
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
            case MYSQL_TYPE_SET:
              out = (val.length() == 1 ? static_cast<int>(val.as_int8()) : val.as_int16());
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
    case MYSQL_TYPE_SET:
      out = (val.length() == 1 ? static_cast<int>(val.as_int8()) : val.as_int16());
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
