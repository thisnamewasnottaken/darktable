#include "StdAfx.h"
#include "CiffEntry.h"
#include <math.h>
/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

    http://www.klauspost.com
*/

namespace RawSpeed {

CiffEntry::CiffEntry() {
  own_data = NULL;
}

CiffEntry::CiffEntry(FileMap* f, uint32 value_data, uint32 offset) {
  own_data = NULL;
  unsigned short p = *(unsigned short*)f->getData(offset);
  tag = (CiffTag) (p & 0x3fff);
  ushort16 datalocation = (p & 0xc000);
  type = (CiffDataType) (p & 0x3800);
  if (datalocation == 0x0000) { // Data is offset in value_data
    count = *(int*)f->getData(offset + 2);
    data_offset = *(uint32*)f->getData(offset + 6) + value_data;
    //fprintf(stderr, "  tag 0x%x, count %d, data_offset %d\n", tag, count, data_offset);
    CHECKSIZE(data_offset);
    data = f->getDataWrt(data_offset);
  } else if (datalocation == 0x4000) { // Data is stored directly in entry
    data = f->getDataWrt(offset + 2);
    //fprintf(stderr, "  tag 0x%x\n", tag);
  } else
    ThrowCPE("Don't understand data location 0x%x\n", datalocation);
}

CiffEntry::CiffEntry(CiffTag _tag, CiffDataType _type, uint32 _count, const uchar8* _data )
{
  tag = _tag;
  type = _type;
  count = _count;
  if (NULL == _data) {
    uint32 bytesize = _count << ciff_datashifts[_type];
    own_data = new uchar8[bytesize];
    memset(own_data,0,bytesize);
    data = own_data;
  } else {
    data = _data; 
    own_data = NULL;
  }
#ifdef _DEBUG
  debug_intVal = 0xC0CAC01A;
  debug_floatVal = sqrtf(-1);

  if (type == CIFF_LONG || type == CIFF_SHORT)
    debug_intVal = getInt();
  if (type == CIFF_FLOAT || type == CIFF_DOUBLE)
    debug_floatVal = getFloat();
#endif
}

CiffEntry::~CiffEntry(void) {
  if (own_data)
    delete[] own_data;
}

bool CiffEntry::isInt() {
  return (type == CIFF_LONG || type == CIFF_SHORT || type ==  CIFF_BYTE);
}

unsigned int CiffEntry::getInt() {
  if (!(type == CIFF_LONG || type == CIFF_SHORT || type == CIFF_BYTE))
    ThrowCPE("CIFF, getInt: Wrong type 0x%x encountered. Expected Long, Short or Byte", type);
  if (type == CIFF_BYTE)
    return getByte();
  if (type == CIFF_SHORT)
    return getShort();
  return (uint32)data[3] << 24 | (uint32)data[2] << 16 | (uint32)data[1] << 8 | (uint32)data[0];
}

unsigned short CiffEntry::getShort() {
  if (type != CIFF_SHORT)
    ThrowCPE("CIFF, getShort: Wrong type 0x%x encountered. Expected Short", type);
  return ((ushort16)data[1] << 8) | (ushort16)data[0];
}

const uint32* CiffEntry::getIntArray() {
  if (type != CIFF_LONG)
    ThrowCPE("CIFF, getIntArray: Wrong type 0x%x encountered. Expected Long", type);
  return (uint32*)&data[0];
}

const ushort16* CiffEntry::getShortArray() {
  if (type != CIFF_SHORT)
    ThrowCPE("CIFF, getShortArray: Wrong type 0x%x encountered. Expected Short", type);
  return (ushort16*)&data[0];
}

uchar8 CiffEntry::getByte() {
  if (type != CIFF_BYTE)
    ThrowCPE("CIFF, getByte: Wrong type 0x%x encountered. Expected Byte", type);
  return data[0];
}

string CiffEntry::getString() {
  if (type != CIFF_ASCII)
    ThrowCPE("CIFF, getString: Wrong type 0x%x encountered. Expected Ascii", type);
  if (!own_data) {
    own_data = new uchar8[count];
    memcpy(own_data, data, count);
    own_data[count-1] = 0;  // Ensure string is not larger than count defines
  }
  return string((const char*)&own_data[0]);
}

bool CiffEntry::isString() {
  return (type == CIFF_ASCII);
}

int CiffEntry::getElementSize() {
  return ciff_datasizes[type];
}

int CiffEntry::getElementShift() {
  return ciff_datashifts[type];
}

void CiffEntry::setData( const void *in_data, uint32 byte_count )
{
  uint32 bytesize = count << ciff_datashifts[type];
  if (byte_count > bytesize)
    ThrowCPE("CIFF, data set larger than entry size given");

  if (!own_data) {
    own_data = new uchar8[bytesize];
    memcpy(own_data, data, bytesize);
  }
  memcpy(own_data, in_data, byte_count);
}

uchar8* CiffEntry::getDataWrt()
{
  if (!own_data) {
    uint32 bytesize = count << ciff_datashifts[type];
    own_data = new uchar8[bytesize];
    memcpy(own_data, data, bytesize);
  }
  return own_data;
}

#ifdef _MSC_VER
#pragma warning(disable: 4996) // this function or variable may be unsafe
#endif

std::string CiffEntry::getValueAsString()
{  
  if (type == CIFF_ASCII)
    return string((const char*)&data[0]);
  char *temp_string = new char[4096];
  if (count == 1) {
    switch (type) {
      case CIFF_LONG:
        sprintf(temp_string, "Long: %u (0x%x)", getInt(), getInt());
        break;
      case CIFF_SHORT:
        sprintf(temp_string, "Short: %u (0x%x)", getInt(), getInt());
        break;
      case CIFF_BYTE:
        sprintf(temp_string, "Byte: %u (0x%x)", getInt(), getInt());
        break;
      default:
        sprintf(temp_string, "Type: %x: ", type);
        for (uint32 i = 0; i < ciff_datasizes[type]; i++) {
          sprintf(&temp_string[strlen(temp_string-1)], "%x", data[i]);
        }
    }
  }
  string ret(temp_string);
  delete temp_string;
  return ret;
}

} // namespace RawSpeed
