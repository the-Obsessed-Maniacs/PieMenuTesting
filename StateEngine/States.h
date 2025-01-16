//
// Statemachine code from reading SCXML file 'States.scxml'
// Created by: The Qt SCXML Compiler version 2 (Qt 6.8.0)
// WARNING! All changes made in this file will be lost!
//

#ifndef STATES_H
#define STATES_H

#include <QScxmlStateMachine>
#include <QString>
#include <QVariant>

class PieMenuState: public QScxmlStateMachine
{
    /* qmake ignore Q_OBJECT */
    Q_OBJECT
    Q_PROPERTY(bool Root)
    Q_PROPERTY(bool MainState)
    Q_PROPERTY(bool hidden)
    Q_PROPERTY(bool still)
    Q_PROPERTY(bool closeBy)
    Q_PROPERTY(bool selected)
    Q_PROPERTY(bool subMenu)
    Q_PROPERTY(bool KeyboardOverride)
    Q_PROPERTY(bool Inactive)
    Q_PROPERTY(bool Active)


public:
    Q_INVOKABLE PieMenuState(QObject *parent = 0);
    ~PieMenuState();



Q_SIGNALS:


private:
    struct Data;
    friend struct Data;
    struct Data *data;
};

Q_DECLARE_METATYPE(::PieMenuState*)

#endif // STATES_H
