/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools projectmay not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "nifmodel.h"
#include "nifproxy.h"
#include "kfmmodel.h"

#include <QItemDelegate>

#include "spellbook.h"

#include "widgets/valueedit.h"

#include <QComboBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

extern void qt_format_text(const QFont& font, const QRectF &_r,
                           int tf, const QString& str, QRectF *brect,
                           int tabstops, int* tabarray, int tabarraylen,
                           QPainter* painter);

class NifDelegate : public QItemDelegate
{
public:
	NifDelegate() : QItemDelegate() {}
	
	virtual bool editorEvent( QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index )
	{
		Q_ASSERT( event );
		Q_ASSERT( model );
		
		switch ( event->type() )
		{
			case QEvent::MouseButtonPress:
			case QEvent::MouseButtonRelease:
				if ( static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton 
					&& decoRect( option ).contains( static_cast<QMouseEvent*>(event)->pos() ) )
				{
					Spell * spell = SpellBook::lookup( model->data( index, Qt::UserRole ).toString() );
					if ( spell && ! spell->icon().isNull() )
					{
						if ( event->type() == QEvent::MouseButtonRelease )
						{
							NifModel * nif = 0;
							QModelIndex buddy = index;
							
							if ( model->inherits( "NifModel" ) )
							{
								nif = static_cast<NifModel *>( model );
							}
							else if ( model->inherits( "NifProxyModel" ) )
							{
								NifProxyModel * proxy = static_cast<NifProxyModel*>( model );
								nif = static_cast<NifModel *>( proxy->model() );
								buddy = proxy->mapTo( index );
							}
							
							if ( nif && spell->isApplicable( nif, buddy ) )
								spell->cast( nif, buddy );
						}
						return true;
					}
				}	break;
			case QEvent::MouseButtonDblClick:
				if ( static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton )
				{
					QVariant v = model->data( index, Qt::EditRole );
					if ( v.canConvert<NifValue>() )
					{
						NifValue nv = v.value<NifValue>();
						if ( nv.type() == NifValue::tBool )
						{
							nv.set<int>( ! nv.get<int>() );
							model->setData( index, nv.toVariant(), Qt::EditRole );
							return true;
						}
					}
				}	break;
			default:
				break;
		}
		return false;
	}
	
	virtual void paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
	{
		QString text = index.data( Qt::DisplayRole ).toString();
		QString deco = index.data( Qt::DecorationRole ).toString();
		
		QString user = index.data( Qt::UserRole ).toString();
		QIcon icon;
		if ( ! user.isEmpty() )
		{
			Spell * spell = SpellBook::lookup( user );
			if ( spell ) icon = spell->icon();
		}
		
		QStyleOptionViewItem opt = option;
		
		QRect tRect = opt.rect;
		QRect dRect;
		
		if ( ! icon.isNull() || ! deco.isEmpty() )
		{
			dRect = decoRect( opt );
			tRect = textRect( opt );
		}
		
		QPalette::ColorGroup cg = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
		
		QVariant color = index.data( Qt::BackgroundColorRole );
		if ( color.canConvert<QColor>() )
			painter->fillRect( option.rect, color.value<QColor>() );
		else if ( option.state & QStyle::State_Selected )
			painter->fillRect( option.rect, option.palette.brush( cg, QPalette::Highlight ) );
		
		painter->save();
		painter->setPen( opt.palette.color( cg, opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text ) );
		painter->setFont( opt.font );
		
		if ( ! icon.isNull() )
			icon.paint( painter, dRect );
		else if ( ! deco.isEmpty() )
			painter->drawText( dRect, opt.decorationAlignment, deco );
		
		if ( ! text.isEmpty() )
		{
			drawDisplay( painter, opt, tRect, text );
			drawFocus( painter, opt, tRect );
		}
		
		painter->restore();
	}

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QString text = index.data( Qt::DisplayRole ).toString();
		QRect textRect( 0, 0, option.fontMetrics.width(text), option.fontMetrics.lineSpacing() * (text.count(QLatin1Char('\n')) + 1) );
		return textRect.size();
	}

	QWidget * createEditor( QWidget * parent, const QStyleOptionViewItem &, const QModelIndex & index ) const
	{
		if ( ! index.isValid() )
			return 0;
		
		QVariant v = index.data( Qt::EditRole );
		QWidget * w = 0;
		
		if ( v.canConvert<NifValue>() )
		{
			NifValue nv = v.value<NifValue>();
			if ( nv.isCount() && index.column() == NifModel::ValueCol && ! NifValue::enumOptions( index.sibling( index.row(), NifModel::TypeCol ).data( Qt::DisplayRole ).toString() ).isEmpty() )
			{
				QComboBox * c = new QComboBox( parent );
				w = c;
				c->setEditable( true );
			}
			else if ( ValueEdit::canEdit( nv.type() ) )
				w = new ValueEdit( parent );
		}
		else if ( v.type() == QVariant::String )
		{
			QLineEdit *le = new QLineEdit(parent);
			le->setFrame(false);
			w = le;
		}
		if ( w ) w->installEventFilter( const_cast<NifDelegate *>( this ) );
		return w;
	}
	
	void setEditorData(QWidget *editor, const QModelIndex &index) const
	{
		ValueEdit * vedit = qobject_cast<ValueEdit*>( editor );
		QComboBox * cedit = qobject_cast<QComboBox*>( editor );
		QLineEdit * ledit = qobject_cast<QLineEdit*>( editor );
		QVariant	v = index.data( Qt::EditRole );
		
		if ( vedit && v.canConvert<NifValue>() )
		{
			vedit->setValue( v.value<NifValue>() );
		}
		else if ( cedit && v.canConvert<NifValue>() && v.value<NifValue>().isCount() )
		{
			cedit->clear();
			QString t = index.sibling( index.row(), NifModel::TypeCol ).data( Qt::DisplayRole ).toString();
			QStringList o = NifValue::enumOptions( t );
			cedit->addItems( o );
			QString x = NifValue::enumOptionName( t, v.value<NifValue>().toCount() );
			if ( ! x.isEmpty() )
				cedit->setCurrentIndex( o.indexOf( x ) );
			else
				cedit->setEditText( QString::number( v.value<NifValue>().toCount() ) );
		}
		else if ( ledit )
		{
			ledit->setText( v.toString() );
		}
	}
	
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
	{
		Q_ASSERT(model);
		ValueEdit * vedit = qobject_cast<ValueEdit*>( editor );
		QComboBox * cedit = qobject_cast<QComboBox*>( editor );
		QLineEdit * ledit = qobject_cast<QLineEdit*>( editor );
		QVariant v;
		if ( vedit )
		{
			v.setValue( vedit->getValue() );
			model->setData( index, v, Qt::EditRole );
		}
		else if ( cedit )
		{
			QString t = index.sibling( index.row(), NifModel::TypeCol ).data( Qt::DisplayRole ).toString();
			QVariant v = index.data( Qt::EditRole );
			bool ok;
			quint32 x = NifValue::enumOptionValue( t, cedit->currentText(), &ok );
			if ( ! ok )
				x = cedit->currentText().toUInt();
			if ( v.canConvert<NifValue>() )
			{
				NifValue nv = v.value<NifValue>();
				nv.setCount( x );
				v.setValue( nv );
				model->setData( index, v, Qt::EditRole );
			}
		}
		else if ( ledit )
		{
			v.setValue( ledit->text() );
			model->setData( index, v, Qt::EditRole );
		}
	}

	QRect decoRect( const QStyleOptionViewItem & opt ) const
	{
		// allways upper left
		return QRect( opt.rect.topLeft(), opt.decorationSize );
	}
	
	QRect textRect( const QStyleOptionViewItem & opt ) const
	{
		return QRect( QPoint( opt.rect.x() + opt.decorationSize.width(), opt.rect.y() ), QSize( opt.rect.width() - opt.decorationSize.width(), opt.rect.height() ) );
	}
	
};

QAbstractItemDelegate * NifModel::createDelegate()
{
	return new NifDelegate;
}

QAbstractItemDelegate * KfmModel::createDelegate()
{
	return new NifDelegate;
}
