#pragma once
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QImage>
#include <QRectF>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <functional>

class Lighttable final:public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(QString selected READ selected WRITE select NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList formats READ formats CONSTANT)
public:
    explicit Lighttable(QString directory,QObject* parent=nullptr);
    ~Lighttable()override;
    int rowCount(const QModelIndex& parent={})const override;
    QVariant data(const QModelIndex&,int role)const override;
    QHash<int,QByteArray> roleNames()const override;
    int count()const{return int(photos_.size());}bool busy()const{return busy_;}
    QString message()const{return message_;}QString selected()const{return selected_;}
    QVariantList formats()const;
    Q_INVOKABLE void select(QString id);
    Q_INVOKABLE void addFiles(QVariantList urls,QString format);
    Q_INVOKABLE void addImage(QImage image,QString name,QString format,QString edge="plain");
    Q_INVOKABLE void move(QString id,double x,double y);
    Q_INVOKABLE void resize(QString id,double factor);
    Q_INVOKABLE void rotate(QString id,double degrees);
    Q_INVOKABLE void remove(QString id);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void arrange();
    Q_INVOKABLE QRectF bounds()const;
    Q_INVOKABLE QVariantMap photo(QString id)const;
    Q_INVOKABLE void saveBoard(QUrl destination);
    Q_INVOKABLE void copyBoard();
    Q_INVOKABLE void flush();
signals:
    void countChanged();void stateChanged();void selectionChanged();
private:
    enum Roles{Id=Qt::UserRole+1,Caption,Source,W,H,X,Y,Angle,Scale,Z};
    struct Photo{QString id,name,format;double w=36,h=24,x=0,y=0,angle=0,scale=1;int z=0;};
    struct Result{QList<Photo> photos;QString message;QImage clipboard;};
    static QSizeF physicalSize(QSize pixels,const QString& format,const QString& edge);
    static Photo cacheImage(QImage image,QString name,QString format,QString edge,const QString& directory);
    static QImage compose(const QList<Photo>& photos,const QString& directory);
    static QRectF boundsOf(const QList<Photo>& photos);
    void start(std::function<Result()> work);
    void finishWork();
    void load();int indexOf(QString id)const;void changed(int index);
    QString directory_,message_,selected_;QList<Photo> photos_;bool busy_=false,storageBlocked_=false;
    QFutureWatcher<Result> watcher_;QTimer saveTimer_;
};
