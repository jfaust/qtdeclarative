/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickcanvasitemnode_p.h"

#include <QtQuick/private/qsgcontext_p.h>
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE


QQuickCanvasItemNode::QQuickCanvasItemNode()
    : QSGGeometryNode()
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    , m_texture(0)
    , m_size(1, 1)
    , m_dirtyGeometry(false)
    , m_dirtyTexture(false)
{
    setMaterial(&m_materialO);
    setOpaqueMaterial(&m_material);
    setGeometry(&m_geometry);
}

QQuickCanvasItemNode::~QQuickCanvasItemNode()
{
    delete m_texture;
}

// Must be called before this node is added to SG
void QQuickCanvasItemNode::setCallback(QQuickCanvasItemNode::Callback *cb)
{
    m_cb = cb;
    setFlag(UsePreprocess, true);
}

void QQuickCanvasItemNode::setSize(const QSizeF& size)
{
    if (m_size != size) {
        m_dirtyGeometry = true;
        m_size = size;
    }
}

void QQuickCanvasItemNode::preprocess()
{
    if (m_cb)
        m_cb->process();
}

void QQuickCanvasItemNode::setTexture(QSGDynamicTexture* texture)
{
    if (texture != m_texture) {
        m_dirtyTexture = true;
        m_texture = texture;
    }
}

void QQuickCanvasItemNode::update()
{
    if (m_dirtyGeometry)
        updateGeometry();
    if (m_dirtyTexture)
        updateTexture();

    m_dirtyGeometry = false;
    m_dirtyTexture = false;
}

void QQuickCanvasItemNode::updateTexture()
{
    m_material.setTexture(m_texture);
    m_materialO.setTexture(m_texture);
    markDirty(DirtyMaterial);
}

void QQuickCanvasItemNode::updateGeometry()
{
    QRectF source = m_texture->normalizedTextureSubRect();
    QSGGeometry::updateTexturedRectGeometry(&m_geometry,
                                            QRectF(0, 0, m_size.width(), m_size.height()),
                                            source);
    markDirty(DirtyGeometry);
}

QT_END_NAMESPACE
