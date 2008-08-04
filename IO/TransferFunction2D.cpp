/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.

   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
	\file		TransferFunction2D.cpp
	\author		Jens Krueger
				SCI Institute
				University of Utah
	\version	1.0
	\date		July 2008
*/

#include "TransferFunction2D.h"
#include <fstream>

using namespace std;

TransferFunction2D::TransferFunction2D(unsigned int iSize)
{
	Resize(iSize);
}

TransferFunction2D::~TransferFunction2D(void)
{
}


void TransferFunction2D::Resize(unsigned int iSize) {
	pColorData.resize(iSize);
}

bool TransferFunction2D::Load(const std::string& filename) {
	ifstream file(filename.c_str());

	if (!file.is_open()) return false;

	unsigned int iSize;
	file >> iSize;
	pColorData.resize(iSize);

	for(unsigned int i=0;i<pColorData.size();++i){
		for(unsigned int j=0;j<4;++j){
			file >> pColorData[i][j];
		}
	}

	return true;
}

bool TransferFunction2D::Save(const std::string& filename) {
	ofstream file(filename.c_str());

	if (!file.is_open()) return false;

	file << pColorData.size() << " ";

	for(unsigned int i=0;i<pColorData.size();++i){
		for(unsigned int j=0;j<4;++j){
			file << pColorData[i][j] << " ";
		}
		file << endl;
	}

	return true;
}

void TransferFunction2D::GetByteArray(unsigned char** pcData, unsigned char cUsedRange) {
	if (*pcData == NULL) *pcData = new unsigned char[pColorData.size()];

	unsigned char *pcDataIterator = *pcData;
	for (unsigned int i = 0;i<pColorData.size();i++) {
		*pcDataIterator++ = (unsigned char)(pColorData[i].r*cUsedRange);
		*pcDataIterator++ = (unsigned char)(pColorData[i].g*cUsedRange);
		*pcDataIterator++ = (unsigned char)(pColorData[i].b*cUsedRange);
		*pcDataIterator++ = (unsigned char)(pColorData[i].a*cUsedRange);
	}
}

void TransferFunction2D::GetShortArray(unsigned short** psData, unsigned short sUsedRange) {
	if (*psData == NULL) *psData = new unsigned short[pColorData.size()];

	unsigned short *psDataIterator = *psData;
	for (unsigned int i = 0;i<pColorData.size();i++) {
		*psDataIterator++ = (unsigned short)(pColorData[i].r*sUsedRange);
		*psDataIterator++ = (unsigned short)(pColorData[i].g*sUsedRange);
		*psDataIterator++ = (unsigned short)(pColorData[i].b*sUsedRange);
		*psDataIterator++ = (unsigned short)(pColorData[i].a*sUsedRange);
	}
}

void TransferFunction2D::GetFloatArray(float** pfData) {
	if (*pfData == NULL) *pfData = new float[pColorData.size()];
	memcpy(*pfData, &pfData[0], sizeof(float)*pColorData.size());
}
