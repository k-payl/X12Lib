#ifndef PROJECTWIDGET_H
#define PROJECTWIDGET_H

#include <QWidget>

class MyFileSystemModel;
namespace engine{
	class Core;
}

namespace Ui {
class ProjectWidget;
}

class ProjectWidget : public QWidget
{
	Q_OBJECT

	MyFileSystemModel *model{};

public:
	explicit ProjectWidget(QWidget *parent = nullptr);
	~ProjectWidget();

private slots:
	void onEngineInited(engine::Core *c);
	void onEngineFree(engine::Core *c);

	void on_pushButton_clicked();

private:
	Ui::ProjectWidget *ui;
};

#endif // PROJECTWIDGET_H
