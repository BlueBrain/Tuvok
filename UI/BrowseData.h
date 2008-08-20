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


//!    File   : BrowseData.h
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : July 2008
//
//!    Copyright (C) 2008 SCI Institute


#pragma once

#ifndef BROWSEDATA_H
#define BROWSEDATA_H

#include "AutoGen/ui_BrowseData.h"
#include "RenderWindow.h"

class BrowseData : public QDialog, protected Ui_BrowseData
{
  Q_OBJECT
  public:
    BrowseData(QDialog* pleaseWaitDialog, QString strDir, QWidget* parent = 0, Qt::WindowFlags flags = 0);
    virtual ~BrowseData() {}

    QString GetFileName() {return m_strFilename;}

    bool DataFound() {return m_bDataFound;}

  private:
    bool  m_bDataFound;
    QString m_strDir;
    QString m_strFilename;


    bool FillTable(QDialog* pleaseWaitDialog);

    virtual void showEvent ( QShowEvent * event );
};


#endif // BROWSEDATA_H
