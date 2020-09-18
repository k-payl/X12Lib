#ifndef TEXTURELINEEDIT_H
#define TEXTURELINEEDIT_H
#include "vmath.h"
#include <QWidget>
#include <QLineEdit>

namespace Ui {
class TextureLineEdit;
}

class TextureLineEdit : public QWidget
{
	Q_OBJECT

public:
	explicit TextureLineEdit(QWidget *parent = nullptr);
	virtual ~TextureLineEdit();

	void SetPath(const char *path);
	void SetUV(const math::vec4& uv);

signals:
	void OnUVChanged(const math::vec4& uv);
	void OnPathChanged(const char *path);

private slots:
	void uvChanged();


private:
	Ui::TextureLineEdit *ui;
};


#endif // TEXTURELINEEDIT_H
