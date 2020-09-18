#ifndef MODELPROPERTYWIDGET_H
#define MODELPROPERTYWIDGET_H

#include <QWidget>
#include <QList>
#include "vmath.h"

class Material;

template<class T>
class TextureLineEditConcrete;

namespace engine {
class Model;

}

namespace Ui {
class ModelPropertyWidget;
}

class ModelPropertyWidget : public QWidget
{
	Q_OBJECT

	engine::Model *model_;
	QList<QMetaObject::Connection> connections_;
	bool needRecreatematerialGroup{};

	void destroy_material_group();
	void construct_material_group(Material *mat);

public:
	explicit ModelPropertyWidget(QWidget *parent, engine::Model *m);
	~ModelPropertyWidget();

private slots:
	void onUpdate(float dt);

private:
	Ui::ModelPropertyWidget *ui;

};

#endif // MODELPROPERTYWIDGET_H
