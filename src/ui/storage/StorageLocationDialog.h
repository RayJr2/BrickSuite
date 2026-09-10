#pragma once

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTextEdit;

class StorageLocationDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode { Add, Edit };
    struct Choice { qint64 id=0; QString name; };
    struct Values {
        QString name; QString description; qint64 parentStorageId=0;
        qint64 storageTypeId=0; bool allowsInventory=true; bool allowsCollection=false;
    };

    StorageLocationDialog(Mode mode,const Values&initial,const QList<Choice>&types,
                          const QList<Choice>&parents,QWidget*parent=nullptr);
    Values values()const;
    void setPending(bool pending);
    void setUnknownOutcome(bool unknown,const QString&message={});
    void showError(const QString&message);
    void completeSuccessfully();

signals:
    void submitRequested();

private:
    void requestSubmit();
    Mode m_mode;
    QLineEdit*m_name=nullptr; QTextEdit*m_description=nullptr;
    QComboBox*m_parent=nullptr; QComboBox*m_type=nullptr;
    QCheckBox*m_inventory=nullptr; QCheckBox*m_collection=nullptr;
    QDialogButtonBox*m_buttons=nullptr; QLabel*m_status=nullptr;
    bool m_pending=false; bool m_unknown=false;
};
