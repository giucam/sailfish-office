/*
 *
 */

#include "pdfpagemodel.h"
#include "pdfdocument.h"
#include "pdfrenderthread.h"

#include <limits>
#include <cmath>

#include <QPixmap>
#include <QThreadPool>
#include <QDebug>

#include <poppler-qt4.h>

class PDFPageModel::Private
{
public:
    Private() : pageWidth{ 64 }, pageCount{ 0 } { }

    QImage bestMatchingImage( int index );

    QHash< int, QHash< uint, QImage > > images;

    uint pageWidth;
    int pageCount;
};

PDFPageModel::PDFPageModel(QObject* parent)
    : QAbstractListModel{ parent }, d{ new Private }
{
    QHash< int, QByteArray > roleNames;
    roleNames.insert( PageRole, "page" );
    roleNames.insert( WidthRole, "width" );
    roleNames.insert( HeightRole, "height" );
    setRoleNames(roleNames);

    connect(PDFRenderThread::instance(), SIGNAL(loadFinished()), SLOT(documentLoaded()));
    if(PDFRenderThread::instance()->isLoaded())
        documentLoaded();

    connect(PDFRenderThread::instance(), SIGNAL(pageFinished(int,QImage)), SLOT(pageFinished(int,QImage)));
}

PDFPageModel::~PDFPageModel()
{
    delete d;
}

QVariant PDFPageModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid())
        return QVariant();

    switch(role)
    {
        case PageRole: {
            QImage img = d->bestMatchingImage( index.row() );
            if( (unsigned)img.width() != d->pageWidth )
                PDFRenderThread::instance()->requestPage( index.row(), d->pageWidth );

            return img;
        }
        case WidthRole:
            return d->pageWidth;
        case HeightRole: {
            QImage img = d->bestMatchingImage( index.row() );
            if( !img.isNull() )
                return img.height();

            img = d->bestMatchingImage( d->images.begin().key() );
            return img.height();
        }
        default:
            break;
    }

    return QVariant();
}

int PDFPageModel::rowCount(const QModelIndex& parent) const
{
    return d->pageCount;
}

uint PDFPageModel::pageWidth() const
{
    return d->pageWidth;
}

void PDFPageModel::setPageWidth( uint pageWidth )
{
    if(pageWidth != d->pageWidth) {
        d->pageWidth = pageWidth;
        emit dataChanged( index( 0, 0 ), index( d->pageCount - 1, 0 ) );
        emit pageWidthChanged();
    }
}

void PDFPageModel::discard(int index)
{
    if( d->images.contains( index ) )
        d->images.remove( index );
}

void PDFPageModel::documentLoaded()
{
    if( d->pageCount > 0 )
    {
        beginRemoveRows(QModelIndex(), 0, d->pageCount);
        endRemoveRows();
    }

    d->images.clear();
    d->pageCount = PDFRenderThread::instance()->pageCount();
    beginInsertRows(QModelIndex(), 0, d->pageCount);
    endInsertRows();
}

void PDFPageModel::pageFinished( int id, QImage image )
{
    if( d->images.contains( id ) )
    {
        d->images[ id ].insert( image.width(), image );
    }
    else
    {
        QHash< uint, QImage > sizes;
        sizes.insert( image.width(), image );
        d->images.insert( id, sizes );
    }

    emit dataChanged( index( id, 0 ), index( id, 0 ) );
}

QImage PDFPageModel::Private::bestMatchingImage(int index)
{
    auto sizes = images.value( index );
    if( sizes.contains( pageWidth ) )
        return sizes.value( pageWidth );

    int target = -1;
    for( uint width : sizes.keys() )
    {
        if( target == -1 || qAbs( width - pageWidth ) < qAbs( target - pageWidth ) )
            target = width;
    }

    if( target != -1 )
        return sizes.value( uint( target ) );

    return QImage();
}