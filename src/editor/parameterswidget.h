#ifndef PARAMETERSWIDGET_H
#define PARAMETERSWIDGET_H

#include <QWidget>
#include <QSet>
#include "vmath.h"
#include "myspinbox.h"

namespace engine {
class GameObject;
}


namespace Ui {
class ParametersWidget;
}

class ParametersWidget : public QWidget
{
	Q_OBJECT

	engine::GameObject *g{nullptr}; //TODO: vector of engine::GameObject for multiselection

	bool enabled_;
	math::vec3 pos_;
	//math::quat rot_;
	math::vec3 sc_;

	QWidget *dynamicWidget = nullptr;

	QList<QDoubleSpinBox*> spinBoxes;

public:
	explicit ParametersWidget(QWidget *parent = nullptr);
	~ParametersWidget();

private:
	Ui::ParametersWidget *ui;

	void clearUI();
	void connectPosition(MySpinBox *w, int xyz_offset);
	void connectRotation(MySpinBox *w, int xyz_offset);
	void connectScale(MySpinBox *w, int xyz_offset);

private slots:
	void OnSelectionChanged(QSet<engine::GameObject*>& objects);
	void OnUpdate(float dt);
};

#endif // PARAMETERSWIDGET_H
