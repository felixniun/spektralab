#pragma once
#include <QAbstractListModel>
#include <QImage>
#include <QVariantMap>

// A thumbnail arriving must update its cell, not reset the whole filmstrip
// and its scroll position. Keep the existing QVariantList API for tests/CLI.
class LibraryModel final:public QAbstractListModel {
public:
    enum {Record=Qt::UserRole+1,Thumbnail};
    int rowCount(const QModelIndex& parent={})const override{return parent.isValid()?0:int(rows_.size());}
    QVariant data(const QModelIndex& index,int role)const override {
        if(!index.isValid()||index.row()<0||index.row()>=rows_.size())return {};
        if(role==Record)return rows_[index.row()];
        if(role==Thumbnail)return rows_[index.row()].value("thumbnail");
        if(role==Qt::DisplayRole)return rows_[index.row()].value("name");return {};
    }
    QHash<int,QByteArray> roleNames()const override{return {{Record,"record"},{Thumbnail,"thumbnailImage"},{Qt::DisplayRole,"display"}};}
    void setRows(const QVariantList& values){
        if(values.size()!=rows_.size()){
            beginResetModel();rows_.clear();for(const auto& value:values)rows_.append(value.toMap());endResetModel();return;
        }
        for(int i=0;i<values.size();++i){const auto row=values[i].toMap();const auto& previous=rows_[i];
            bool changed=row.value("thumbnail").value<QImage>().cacheKey()!=previous.value("thumbnail").value<QImage>().cacheKey();
            for(const auto* key:{"name","url","active","selected","error","aspect","cameraThumbnail"})changed=changed||row.value(key)!=previous.value(key);
            if(changed){rows_[i]=row;emit dataChanged(index(i),index(i),{Record,Thumbnail,Qt::DisplayRole});}
        }
    }
private:
    QList<QVariantMap> rows_;
};
