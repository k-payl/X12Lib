#ifndef DEBUGWIDGET_H
#define DEBUGWIDGET_H

#include <QWidget>
#include <QSet>

namespace engine {
class GameObject;
class Core;
}


namespace Ui {
class debugwidget;
}

class debugwidget : public QWidget
{
	Q_OBJECT

public:
	explicit debugwidget(QWidget *parent = nullptr);
	~debugwidget();

private:
	Ui::debugwidget *ui;

private slots:
	void OnSelectionChanged(QSet<engine::GameObject*>& objects);
	void OnEngineFree(engine::Core *c);
	void OnEngineInit(engine::Core *c);
//	void sliderOChanged(int i);
//	void slider1Changed(int i);
};

#endif // DEBUGWIDGET_H
