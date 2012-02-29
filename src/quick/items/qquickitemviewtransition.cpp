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

#include "qquickitemviewtransition_p.h"
#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qdeclarativetransition_p.h>

QT_BEGIN_NAMESPACE

static QList<int> qquickitemviewtransition_emptyIndexes = QList<int>();
static QList<QObject *> qquickitemviewtransition_emptyTargets = QList<QObject *>();


class QQuickItemViewTransitionJob : public QDeclarativeTransitionManager
{
public:
    QQuickItemViewTransitionJob();
    ~QQuickItemViewTransitionJob();

    void startTransition(QQuickViewItem *item, QQuickItemViewTransitioner *transitioner, QQuickItemViewTransitioner::TransitionType type, const QPointF &to, bool isTargetItem);

    QQuickItemViewTransitioner *m_transitioner;
    QQuickViewItem *m_item;
    QPointF m_toPos;
    QQuickItemViewTransitioner::TransitionType m_type;
    bool m_isTarget;

protected:
    virtual void finished();
};


QQuickItemViewTransitionJob::QQuickItemViewTransitionJob()
    : m_transitioner(0)
    , m_item(0)
    , m_type(QQuickItemViewTransitioner::NoTransition)
    , m_isTarget(false)
{
}

QQuickItemViewTransitionJob::~QQuickItemViewTransitionJob()
{
    if (m_transitioner)
        m_transitioner->runningJobs.remove(this);
}

void QQuickItemViewTransitionJob::startTransition(QQuickViewItem *item, QQuickItemViewTransitioner *transitioner, QQuickItemViewTransitioner::TransitionType type, const QPointF &to, bool isTargetItem)
{
    if (type == QQuickItemViewTransitioner::NoTransition)
        return;
    if (!item) {
        qWarning("startTransition(): invalid item");
        return;
    }
    if (!transitioner) {
        qWarning("startTransition(): invalid transitioner");
        return;
    }

    QDeclarativeTransition *trans = transitioner->transitionObject(type, isTargetItem);
    if (!trans) {
        qWarning("QQuickItemView: invalid view transition!");
        return;
    }

    m_item = item;
    m_transitioner = transitioner;
    m_toPos = to;
    m_type = type;
    m_isTarget = isTargetItem;

    QQuickViewTransitionAttached *attached =
            static_cast<QQuickViewTransitionAttached*>(qmlAttachedPropertiesObject<QQuickViewTransitionAttached>(trans));
    if (attached) {
        attached->m_index = item->index;
        attached->m_item = item->item;
        attached->m_destination = to;
        attached->m_targetIndexes = m_transitioner->targetIndexes(type);
        attached->m_targetItems = m_transitioner->targetItems(type);
        emit attached->indexChanged();
        emit attached->itemChanged();
        emit attached->destinationChanged();
        emit attached->targetIndexesChanged();
        emit attached->targetItemsChanged();
    }

    QDeclarativeStateOperation::ActionList actions;
    actions << QDeclarativeAction(item->item, QLatin1String("x"), QVariant(to.x()));
    actions << QDeclarativeAction(item->item, QLatin1String("y"), QVariant(to.y()));

    m_transitioner->runningJobs << this;
    QDeclarativeTransitionManager::transition(actions, trans, item->item);
}

void QQuickItemViewTransitionJob::finished()
{
    QDeclarativeTransitionManager::finished();

    if (m_transitioner)
        m_transitioner->finishedTransition(this, m_item);

    m_item = 0;
    m_toPos.setX(0);
    m_toPos.setY(0);
    m_type = QQuickItemViewTransitioner::NoTransition;
    m_isTarget = false;
}


QQuickItemViewTransitioner::QQuickItemViewTransitioner()
    : populateTransition(0)
    , addTransition(0), addDisplacedTransition(0)
    , moveTransition(0), moveDisplacedTransition(0)
    , removeTransition(0), removeDisplacedTransition(0)
    , displacedTransition(0)
    , changeListener(0)
    , usePopulateTransition(false)
{
}

QQuickItemViewTransitioner::~QQuickItemViewTransitioner()
{
    for (QSet<QQuickItemViewTransitionJob *>::iterator it = runningJobs.begin(); it != runningJobs.end(); ++it)
        (*it)->m_transitioner = 0;
}

bool QQuickItemViewTransitioner::canTransition(QQuickItemViewTransitioner::TransitionType type, bool asTarget) const
{
    if (!asTarget
            && type != QQuickItemViewTransitioner::NoTransition && type != QQuickItemViewTransitioner::PopulateTransition
            && displacedTransition && displacedTransition->enabled()) {
        return true;
    }

    switch (type) {
    case QQuickItemViewTransitioner::NoTransition:
        break;
    case QQuickItemViewTransitioner::PopulateTransition:
        return usePopulateTransition
                && populateTransition && populateTransition->enabled();
    case QQuickItemViewTransitioner::AddTransition:
        if (asTarget)
            return addTransition && addTransition->enabled();
        else
            return addDisplacedTransition && addDisplacedTransition->enabled();
    case QQuickItemViewTransitioner::MoveTransition:
        if (asTarget)
            return moveTransition && moveTransition->enabled();
        else
            return moveDisplacedTransition && moveDisplacedTransition->enabled();
    case QQuickItemViewTransitioner::RemoveTransition:
        if (asTarget)
            return removeTransition && removeTransition->enabled();
        else
            return removeDisplacedTransition && removeDisplacedTransition->enabled();
    }
    return false;
}

void QQuickItemViewTransitioner::transitionNextReposition(QQuickViewItem *item, QQuickItemViewTransitioner::TransitionType type, bool isTarget)
{
    bool matchedTransition = false;
    if (type == QQuickItemViewTransitioner::AddTransition) {
        // don't run add transitions for added items while populating
        if (usePopulateTransition)
            matchedTransition = false;
        else
            matchedTransition = canTransition(type, isTarget);
    } else {
        matchedTransition = canTransition(type, isTarget);
    }

    if (matchedTransition) {
        item->setNextTransition(type, isTarget);
    } else {
        // the requested transition type is not valid, but the item is scheduled/in another
        // transition, so cancel it to allow the item to move directly to the correct pos
        if (item->transitionScheduledOrRunning())
            item->stopTransition();
    }
}

QDeclarativeTransition *QQuickItemViewTransitioner::transitionObject(QQuickItemViewTransitioner::TransitionType type, bool asTarget)
{
    if (type == QQuickItemViewTransitioner::NoTransition)
        return 0;

    if (type == PopulateTransition)
        asTarget = true;    // no separate displaced transition

    QDeclarativeTransition *trans = 0;
    switch (type) {
    case NoTransition:
        break;
    case PopulateTransition:
        trans = populateTransition;
        break;
    case AddTransition:
        trans = asTarget ? addTransition : addDisplacedTransition;
        break;
    case MoveTransition:
        trans = asTarget ? moveTransition : moveDisplacedTransition;
        break;
    case RemoveTransition:
        trans = asTarget ? removeTransition : removeDisplacedTransition;
        break;
    }

    if (!asTarget && (!trans || !trans->enabled()))
        trans = displacedTransition;
    if (trans && trans->enabled())
        return trans;
    return 0;
}

const QList<int> &QQuickItemViewTransitioner::targetIndexes(QQuickItemViewTransitioner::TransitionType type) const
{
    switch (type) {
    case QQuickItemViewTransitioner::NoTransition:
        break;
    case QQuickItemViewTransitioner::PopulateTransition:
    case QQuickItemViewTransitioner::AddTransition:
        return addTransitionIndexes;
    case QQuickItemViewTransitioner::MoveTransition:
        return moveTransitionIndexes;
    case QQuickItemViewTransitioner::RemoveTransition:
        return removeTransitionIndexes;
    }

    return qquickitemviewtransition_emptyIndexes;
}

const QList<QObject *> &QQuickItemViewTransitioner::targetItems(QQuickItemViewTransitioner::TransitionType type) const
{
    switch (type) {
    case QQuickItemViewTransitioner::NoTransition:
        break;
    case QQuickItemViewTransitioner::PopulateTransition:
    case QQuickItemViewTransitioner::AddTransition:
        return addTransitionTargets;
    case QQuickItemViewTransitioner::MoveTransition:
        return moveTransitionTargets;
    case QQuickItemViewTransitioner::RemoveTransition:
        return removeTransitionTargets;
    }

    return qquickitemviewtransition_emptyTargets;
}

void QQuickItemViewTransitioner::finishedTransition(QQuickItemViewTransitionJob *job, QQuickViewItem *item)
{
    if (!runningJobs.contains(job))
        return;
    runningJobs.remove(job);
    if (item) {
        item->finishedTransition();
        if (changeListener)
            changeListener->viewItemTransitionFinished(item);
    }
}


QQuickViewItem::QQuickViewItem(QQuickItem *i)
    : item(i)
    , transition(0)
    , nextTransitionType(QQuickItemViewTransitioner::NoTransition)
    , index(-1)
    , isTransitionTarget(false)
    , nextTransitionToSet(false)
{
}

QQuickViewItem::~QQuickViewItem()
{
    delete transition;
}

qreal QQuickViewItem::itemX() const
{
    if (nextTransitionType != QQuickItemViewTransitioner::NoTransition)
        return nextTransitionToSet ? nextTransitionTo.x() : item->x();
    else if (transition && transition->isRunning())
        return transition->m_toPos.x();
    else
        return item->x();
}

qreal QQuickViewItem::itemY() const
{
    // If item is transitioning to some pos, return that dest pos.
    // If item was redirected to some new pos before the current transition finished,
    // return that new pos.
    if (nextTransitionType != QQuickItemViewTransitioner::NoTransition)
        return nextTransitionToSet ? nextTransitionTo.y() : item->y();
    else if (transition && transition->isRunning())
        return transition->m_toPos.y();
    else
        return item->y();
}

void QQuickViewItem::moveTo(const QPointF &pos)
{
    if (transitionScheduledOrRunning()) {
        nextTransitionTo = pos;
        nextTransitionToSet = true;
    } else {
        item->setPos(pos);
    }
}

void QQuickViewItem::setVisible(bool visible)
{
    if (!visible && transitionScheduledOrRunning())
        return;
    item->setVisible(visible);
}

bool QQuickViewItem::transitionScheduledOrRunning() const
{
    return (transition && transition->isRunning())
            || nextTransitionType != QQuickItemViewTransitioner::NoTransition;
}

bool QQuickViewItem::transitionRunning() const
{
    return (transition && transition->isRunning());
}

bool QQuickViewItem::isPendingRemoval() const
{
    if (nextTransitionType == QQuickItemViewTransitioner::RemoveTransition)
        return isTransitionTarget;
    if (transition && transition->isRunning() && transition->m_type == QQuickItemViewTransitioner::RemoveTransition)
        return transition->m_isTarget;
    return false;
}

bool QQuickViewItem::prepareTransition(const QRectF &viewBounds)
{
    bool doTransition = false;

    // If item is not already moving somewhere, set it to not move anywhere.
    // This ensures that removed targets don't transition to the default (0,0) and that
    // items set for other transition types only transition if they actually move somewhere.
    if (nextTransitionType != QQuickItemViewTransitioner::NoTransition && !nextTransitionToSet)
        moveTo(item->pos());

    // For move transitions (both target and displaced) and displaced transitions of other
    // types, only run the transition if the item is actually moving to another position.

    switch (nextTransitionType) {
    case QQuickItemViewTransitioner::NoTransition:
    {
        return false;
    }
    case QQuickItemViewTransitioner::PopulateTransition:
    {
        return true;
    }
    case QQuickItemViewTransitioner::AddTransition:
    case QQuickItemViewTransitioner::RemoveTransition:
        if (viewBounds.isNull()) {
            if (isTransitionTarget)
                doTransition = true;
            else
                doTransition = transitionWillChangePosition();
        } else if (isTransitionTarget) {
            // For Add targets, do transition if item is moving into visible area
            // For Remove targets, do transition if item is currently in visible area
            doTransition = (nextTransitionType == QQuickItemViewTransitioner::AddTransition)
                    ? viewBounds.intersects(QRectF(nextTransitionTo.x(), nextTransitionTo.y(), item->width(), item->height()))
                    : viewBounds.intersects(QRectF(item->x(), item->y(), item->width(), item->height()));
            if (!doTransition)
                item->setPos(nextTransitionTo);
        } else {
            if (viewBounds.intersects(QRectF(item->x(), item->y(), item->width(), item->height()))
                    || viewBounds.intersects(QRectF(nextTransitionTo.x(), nextTransitionTo.y(), item->width(), item->height()))) {
                doTransition = transitionWillChangePosition();
            } else {
                item->setPos(nextTransitionTo);
            }
        }
        break;
    case QQuickItemViewTransitioner::MoveTransition:
        // do transition if moving from or into visible area
        if (transitionWillChangePosition()) {
            doTransition = viewBounds.isNull()
                    || viewBounds.intersects(QRectF(item->x(), item->y(), item->width(), item->height()))
                    || viewBounds.intersects(QRectF(nextTransitionTo.x(), nextTransitionTo.y(), item->width(), item->height()));
            if (!doTransition)
                item->setPos(nextTransitionTo);
        }
        break;
    }

    if (!doTransition)
        resetTransitionData();
    return doTransition;
}

void QQuickViewItem::startTransition(QQuickItemViewTransitioner *transitioner)
{
    if (nextTransitionType == QQuickItemViewTransitioner::NoTransition)
        return;

    if (!transition || transition->m_type != nextTransitionType || transition->m_isTarget != isTransitionTarget) {
        delete transition;
        transition = new QQuickItemViewTransitionJob;
    }

    // if item is not already moving somewhere, set it to not move anywhere
    // so that removed items do not move to the default (0,0)
    if (!nextTransitionToSet)
        moveTo(item->pos());

    transition->startTransition(this, transitioner, nextTransitionType, nextTransitionTo, isTransitionTarget);
    nextTransitionType = QQuickItemViewTransitioner::NoTransition;
}

void QQuickViewItem::stopTransition()
{
    if (transition) {
        transition->cancel();
        delete transition;
        transition = 0;
    }
    resetTransitionData();
    finishedTransition();
}

void QQuickViewItem::setNextTransition(QQuickItemViewTransitioner::TransitionType type, bool isTargetItem)
{
    // Don't reset nextTransitionToSet - once it is set, it cannot be changed
    // until the animation finishes since the itemX() and itemY() may be used
    // to calculate positions for transitions for other items in the view.
    nextTransitionType = type;
    isTransitionTarget = isTargetItem;
}

bool QQuickViewItem::transitionWillChangePosition() const
{
    if (transitionRunning() && transition->m_toPos != nextTransitionTo)
        return true;
    return nextTransitionTo != item->pos();
}

void QQuickViewItem::finishedTransition()
{
    nextTransitionToSet = false;
    nextTransitionTo = QPointF();
}

void QQuickViewItem::resetTransitionData()
{
    nextTransitionType = QQuickItemViewTransitioner::NoTransition;
    isTransitionTarget = false;
    nextTransitionTo = QPointF();
    nextTransitionToSet = false;
}


QQuickViewTransitionAttached::QQuickViewTransitionAttached(QObject *parent)
    : QObject(parent), m_item(0), m_index(-1)
{
}
/*!
    \qmlclass ViewTransition QQuickViewTransitionAttached
    \inqmlmodule QtQuick 2
    \ingroup qml-view-elements
    \brief The ViewTransition attached property provides details on items under transition in a view.

    With ListView and GridView, it is possible to specify transitions that should be applied whenever
    the items in the view change as a result of modifications to the view's model. They both have the
    following properties that can be set to the appropriate transitions to be run for various
    operations:

    \list
    \o \c populate - the transition to run when a view is created, or when the model changes
    \o \c add - the transition to apply to items that are added to the view
    \o \c remove - the transition to apply to items that are removed from the view
    \o \c move - the transition to apply to items that are moved within the view (i.e. as a result
       of a move operation in the model)
    \o \c displaced - the generic transition to be applied to any items that are displaced by an
       add, move or remove operation
    \o \c addDisplaced, \c removeDisplaced and \c moveDisplaced - the transitions to be applied when
       items are displaced by add, move, or remove operations, respectively (these override the
       generic displaced transition if specified)
    \endlist

    For the \l Row, \l Column, \l Grid and \l Flow positioner elements, which operate with collections of child
    items rather than data models, the following properties are used instead:

    \list
    \o \c add - the transition to apply to items that are created for the positioner, added to
       or reparented to the positioner, or items that have become \l {Item::}{visible}
    \o \c move - the transition to apply to items that have moved within the positioner, including
       when they are displaced due to the addition or removal of other items, or when items are otherwise
       rearranged within the positioner, or when items are repositioned due to the resizing of other
       items in the positioner
    \endlist

    View transitions have access to a ViewTransition attached property that
    provides details of the items that are under transition and the operation that triggered the
    transition. Since view transitions are run once per item, these details can be used to customise
    each transition for each individual item.

    The ViewTransition attached property provides the following properties specific to the item to
    which the transition is applied:

    \list
    \o ViewTransition.item - the item that is under transition
    \o ViewTransition.index - the index of this item
    \o ViewTransition.destination - the (x,y) point to which this item is moving for the relevant view operation
    \endlist

    In addition, ViewTransition provides properties specific to the items which are the target
    of the operation that triggered the transition:

    \list
    \o ViewTransition.targetIndexes - the indexes of the target items
    \o ViewTransition.targetItems - the target items themselves
    \endlist

    (Note that for the \l Row, \l Column, \l Grid and \l Flow positioner elements, the \c move transition only
    provides these two additional details when the transition is triggered by the addition of items
    to a positioner.)

    View transitions can be written without referring to any of the attributes listed
    above. These attributes merely provide extra details that are useful for customising view
    transitions.

    Following is an introduction to view transitions and the ways in which the ViewTransition
    attached property can be used to augment view transitions.


    \section2 View transitions: a simple example

    Here is a basic example of the use of view transitions. The view below specifies transitions for
    the \c add and \c displaced properties, which will be run when items are added to the view:

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-basic.qml 0

    When the space key is pressed, adding an item to the model, the new item will fade in and
    increase in scale over 400 milliseconds as it is added to the view. Also, any item that is
    displaced by the addition of a new item will animate to its new position in the view over
    400 milliseconds, as specified by the \c displaced transition.

    If five items were inserted in succession at index 0, the effect would be this:

    \image viewtransitions-basic.gif

    Notice that the NumberAnimation objects above do not need to specify a \c target to animate
    the appropriate item. Also, the NumberAnimation in the \c addTransition does not need to specify
    the \c to value to move the item to its correct position in the view. This is because the view
    implicitly sets the \c target and \c to values with the correct item and final item position
    values if these properties are not explicitly defined.

    At its simplest, a view transition may just animate an item to its new position following a
    view operation, just as the \c displaced transition does above, or animate some item properties,
    as in the \c add transition above. Additionally, a view transition may make use of the
    ViewTransition attached property to customise animation behavior for different items. Following
    are some examples of how this can be achieved.


    \section2 Using the ViewTransition attached property

    As stated, the various ViewTransition properties provide details specific to the individual item
    being transitioned as well as the operation that triggered the transition. In the animation above,
    five items are inserted in succession at index 0. When the fifth and final insertion takes place,
    adding "Item 4" to the view, the \c add transition is run once (for the inserted item) and the
    \c displaced transition is run four times (once for each of the four existing items in the view).

    At this point, if we examined the \c displaced transition that was run for the bottom displaced
    item ("Item 0"), the ViewTransition property values provided to this transition would be as follows:

    \table
    \header
        \o Property
        \o Value
        \o Explanation
    \row
        \o ViewTransition.item
        \o "Item 0" delegate instance
        \o The "Item 0" \l Rectangle object itself
    \row
        \o ViewTransition.index
        \o \c int value of 4
        \o The index of "Item 0" within the model following the add operation
    \row
        \o ViewTransition.destination
        \o \l point value of (0, 120)
        \o The position that "Item 0" is moving to
    \row
        \o ViewTransition.targetIndexes
        \o \c int array, just contains the integer "0" (zero)
        \o The index of "Item 4", the new item added to the view
    \row
        \o ViewTransition.targetItems
        \o object array, just contains the "Item 4" delegate instance
        \o The "Item 4" \l Rectangle object - the new item added to the view
    \endtable

    The ViewTransition.targetIndexes and ViewTransition.targetItems lists provide the items and
    indexes of all delegate instances that are the targets of the relevant operation. For an add
    operation, these are all the items that are added into the view; for a remove, these are all
    the items removed from the view, and so on. (Note these lists will only contain references to
    items that have been created within the view or its cached items; targets that are not within
    the visible area of the view or within the item cache will not be accessible.)

    So, while the ViewTransition.item, ViewTransition.index and ViewTransition.destination values
    vary for each individual transition that is run, the ViewTransition.targetIndexes and
    ViewTransition.targetItems values are the same for every \c add and \c displaced transition
    that is triggered by a particular add operation.


    \section3 Delaying animations based on index

    Since each view transition is run once for each item affected by the transition, the ViewTransition
    properties can be used within a transition to define custom behavior for each item's transition.
    For example, the ListView in the previous example could use this information to create a ripple-type
    effect on the movement of the displaced items.

    This can be achieved by modifying the \c displaced transition so that it delays the animation of
    each displaced item based on the difference between its index (provided by ViewTransition.index)
    and the first removed index (provided by ViewTransition.targetIndexes):

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-delayedbyindex.qml 0

    Each displaced item delays its animation by an additional 100 milliseconds, producing a subtle
    ripple-type effect when items are displaced by the add, like this:

    \image viewtransitions-delayedbyindex.gif


    \section3 Animating items to intermediate positions

    The ViewTransition.item property gives a reference to the item to which the transition is being
    applied. This can be used to access any of the item's attributes, custom \c property values,
    and so on.

    Below is a modification of the \c displaced transition from the previous example. It adds a
    ParallelAnimation with nested NumberAnimation objects that reference ViewTransition.item to access
    each item's \c x and \c y values at the start of their transitions. This allows each item to
    animate to an intermediate position relative to its starting point for the transition, before
    animating to its final position in the view:

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-intermediatemove.qml 0

    Now, a displaced item will first move to a position of (20, 50) relative to its starting
    position, and then to its final, correct position in the view:

    \image viewtransitions-intermediatemove.gif

    Since the final NumberAnimation does not specify a \c to value, the view implicitly sets this
    value to the item's final position in the view, and so this last animation will move this item
    to the correct place. If the transition requires the final position of the item for some calculation,
    this is accessible through ViewTransition.destination.

    Instead of using multiple NumberAnimations, you could use a PathAnimation to animate an item over
    a curved path. For example, the \c add transition in the previous example could be augmented with
    a PathAnimation as follows: to animate newly added items along a path:

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-pathanim.qml 0

    This animates newly added items along a path. Notice that each path is specified relative to
    each item's final destination point, so that items inserted at different indexes start their
    paths from different positions:

    \image viewtransitions-pathanim.gif


    \section2 Handling interrupted animations

    A view transition may be interrupted at any time if a different view transition needs to be
    applied while the original transition is in progress. For example, say Item A is inserted at index 0
    and undergoes an "add" transition; then, Item B is inserted at index 0 in quick succession before
    Item A's transition has finished. Since Item B is inserted before Item A, it will displace Item
    A, causing the view to interrupt Item A's "add" transition mid-way and start a "displaced"
    transition on Item A instead.

    For simple animations that simply animate an item's movement to its final destination, this
    interruption is unlikely to require additional consideration. However, if a transition changes other
    properties, this interruption may cause unwanted side effects. Consider the first example on this
    page, repeated below for convenience:

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-basic.qml 0

    If multiple items are added in rapid succession, without waiting for a previous transition
    to finish, this is the result:

    \image viewtransitions-interruptedbad.gif

    Each newly added item undergoes an \c add transition, but before the transition can finish,
    another item is added, displacing the previously added item. Because of this, the \c add
    transition on the previously added item is interrupted and a \c displaced transition is
    started on the item instead. Due to the interruption, the \c opacity and \c scale animations
    have not completed, thus producing items with opacity and scale that are below 1.0.

    To fix this, the \c displaced transition should additionally ensure the item properties are
    set to the end values specified in the \c add transition, effectively resetting these values
    whenever an item is displaced. In this case, it means setting the item opacity and scale to 1.0:

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-interruptedgood.qml 0

    Now, when an item's \c add transition is interrupted, its opacity and scale are animated to 1.0
    upon displacement, avoiding the erroneous visual effects from before:

    \image viewtransitions-interruptedgood.gif

    The same principle applies to any combination of view transitions. An added item may be moved
    before its add transition finishes, or a moved item may be removed before its moved transition
    finishes, and so on; so, the rule of thumb is that every transition should handle the same set of
    properties.


    \section2 Restrictions regarding ScriptAction

    When a view transition is initialized, any property bindings that refer to the ViewTransition
    attached property are evaluated in preparation for the transition. Due to the nature of the
    internal construction of a view transition, the attributes of the ViewTransition attached
    property are only valid for the relevant item when the transition is initialized, and may not be
    valid when the transition is actually run.

    Therefore, a ScriptAction within a view transition should not refer to the ViewTransition
    attached property, as it may not refer to the expected values at the time that the ScriptAction
    is actually invoked. Consider the following example:

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-scriptactionbad.qml 0

    When the space key is pressed, three items are moved from index 5 to index 1. For each moved
    item, the \c moveTransition sequence presumably animates the item's color to "yellow", then
    animates it to its final position, then changes the item color back to "lightsteelblue" using a
    ScriptAction. However, when run, the transition does not produce the intended result:

    \image viewtransitions-scriptactionbad.gif

    Only the last moved item is returned to the "lightsteelblue" color; the others remain yellow. This
    is because the ScriptAction is not run until after the transition has already been initialized, by
    which time the ViewTransition.item value has changed to refer to a different item; the item that
    the script had intended to refer to is not the one held by ViewTransition.item at the time the
    ScriptAction is actually invoked.

    In this instance, to avoid this issue, the view could set the property using a PropertyAction
    instead:

    \snippet doc/src/snippets/declarative/viewtransitions/viewtransitions-scriptactiongood.qml 0

    When the transition is initialized, the PropertyAction \c target will be set to the respective
    ViewTransition.item for the transition and will later run with the correct item target as
    expected.
  */

/*!
    \qmlattachedproperty list QtQuick2::ViewTransition::index

    This attached property holds the index of the item that is being
    transitioned.

    Note that if the item is being moved, this property holds the index that
    the item is moving to, not from.
*/

/*!
    \qmlattachedproperty list QtQuick2::ViewTransition::item

    This attached property holds the the item that is being transitioned.

    \warning This item should not be kept and referred to outside of the transition
    as it may become invalid as the view changes.
*/

/*!
    \qmlattachedproperty list QtQuick2::ViewTransition::destination

    This attached property holds the final destination position for the transitioned
    item within the view.

    This property value is a \l point with \c x and \c y properties.
*/

/*!
    \qmlattachedproperty list QtQuick2::ViewTransition::targetIndexes

    This attached property holds a list of the indexes of the items in view
    that are the target of the relevant operation.

    The targets are the items that are the subject of the operation. For
    an add operation, these are the items being added; for a remove, these
    are the items being removed; for a move, these are the items being
    moved.

    For example, if the transition was triggered by an insert operation
    that added two items at index 1 and 2, this targetIndexes list would
    have the value [1,2].

    \note The targetIndexes list only contains the indexes of items that are actually
    in view, or will be in the view once the relevant operation completes.

    \sa QtQuick2::ViewTransition::targetIndexes
*/

/*!
    \qmlattachedproperty list QtQuick2::ViewTransition::targetItems

    This attached property holds the list of items in view that are the
    target of the relevant operation.

    The targets are the items that are the subject of the operation. For
    an add operation, these are the items being added; for a remove, these
    are the items being removed; for a move, these are the items being
    moved.

    For example, if the transition was triggered by an insert operation
    that added two items at index 1 and 2, this targetItems list would
    contain these two items.

    \note The targetItems list only contains items that are actually
    in view, or will be in the view once the relevant operation completes.

    \warning The objects in this list should not be kept and referred to
    outside of the transition as the items may become invalid. The targetItems
    are only valid when the Transition is initially created; this also means
    they should not be used by ScriptAction objects in the Transition, which are
    not evaluated until the transition is run.

    \sa QtQuick2::ViewTransition::targetIndexes
*/
QDeclarativeListProperty<QObject> QQuickViewTransitionAttached::targetItems()
{
    return QDeclarativeListProperty<QObject>(this, m_targetItems);
}

QQuickViewTransitionAttached *QQuickViewTransitionAttached::qmlAttachedProperties(QObject *obj)
{
    return new QQuickViewTransitionAttached(obj);
}

QT_END_NAMESPACE