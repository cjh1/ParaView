/*=========================================================================

   Program: ParaView
   Module: pqPropertyWidget.cxx

   Copyright (c) 2005-2012 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include "pqPropertyWidget.h"

#include "pqProxy.h"

pqPropertyWidget::pqPropertyWidget(vtkSMProxy *proxy, QWidget *parent)
  : QWidget(parent),
    Proxy(proxy),
    Property(0)
{
  this->Modified = false;
  this->Links.setAutoUpdateVTKObjects(false);
  this->Links.setUseUncheckedProperties(true);
  this->connect(&this->Links, SIGNAL(qtWidgetChanged()), this, SIGNAL(modified()));
  this->connect(this, SIGNAL(modified()), this, SLOT(setModified()));
}

pqPropertyWidget::~pqPropertyWidget()
{
}

void pqPropertyWidget::setProxy(vtkSMProxy *proxy)
{
  this->Proxy = proxy;
}

vtkSMProxy* pqPropertyWidget::proxy() const
{
  return this->Proxy;
}

void pqPropertyWidget::setProperty(vtkSMProperty *property)
{
  this->Property = property;
}

vtkSMProperty* pqPropertyWidget::property() const
{
  return this->Property;
}

void pqPropertyWidget::apply()
{
  this->Links.accept();
  this->setModified(false);
}

void pqPropertyWidget::reset()
{
  this->Links.reset();
  this->setModified(false);
}

bool pqPropertyWidget::showLabel() const
{
  return true;
}

void pqPropertyWidget::addPropertyLink(QObject *qobject,
                                       const char *qproperty,
                                       const char *qsignal,
                                       vtkSMProperty *smproperty,
                                       int smindex)
{
  this->Links.addPropertyLink(qobject,
                              qproperty,
                              qsignal,
                              this->Proxy,
                              smproperty,
                              smindex);
}

void pqPropertyWidget::setModified(bool modified)
{
  this->Modified = modified;
}

bool pqPropertyWidget::isModified() const
{
  return this->Modified;
}

void pqPropertyWidget::setAutoUpdateVTKObjects(bool autoUpdate)
{
  this->Links.setAutoUpdateVTKObjects(autoUpdate);
}

void pqPropertyWidget::setUseUncheckedProperties(bool useUnchecked)
{
  this->Links.setUseUncheckedProperties(useUnchecked);
}
