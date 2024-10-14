#include "nifskope.h"
#include "spellbook.h"

#include <QDirIterator>
#include <QFileDialog>

//! Bulk Open and Save NIF Files
class spBulkOpenSave final : public Spell
{
public:
    //Spell Implementation
    QString name() const override final { return Spell::tr( "Bulk Open and Save" ); }
    QString page() const override final { return Spell::tr( "" ); }
    QIcon icon() const override final
    {
        return QIcon();
    }
    bool constant() const override final { return false; }
    bool instant() const override final { return true; }

    bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
    {
        return ( nif && !index.isValid() );
    }

    QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final;
    //End Spell Implementation
};

QModelIndex spBulkOpenSave::cast( NifModel * nif, const QModelIndex & index )
{
    if ( !nif )
        return index;

    // Reference to NifSkope required for successful saving
    NifSkope* nifSkope = qobject_cast<NifSkope*>(nif->getWindow());
    if ( !nifSkope )
        return index;

    QString rootFolder = QFileDialog::getExistingDirectory(nullptr, "Select root folder to process");
    if ( rootFolder.isEmpty() ) {
        return index;
    }

    QDirIterator it(rootFolder, QStringList() << "*.nif", QDir::Files, QDirIterator::Subdirectories);
    while ( it.hasNext() ) {
        QString filePath = it.next();

        // Open NIF
        nifSkope->openFile(filePath);
        QCoreApplication::processEvents();

        // Save NIF
        nifSkope->publicSave();
        QCoreApplication::processEvents();
    }

    return index;
}

REGISTER_SPELL(spBulkOpenSave)