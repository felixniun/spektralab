#pragma once
#include "desktop/preview_host.hpp"
#include "FrameOutput.hpp"
#include "EditStore.hpp"
#include "LibraryModel.hpp"
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

class PreviewController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY frameChanged)
    Q_PROPERTY(bool sourceIsRaw READ sourceIsRaw NOTIFY frameChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY settingsChanged)
    Q_PROPERTY(bool settingsEditable READ settingsEditable NOTIFY stateChanged)
    Q_PROPERTY(bool filmIsPositive READ filmIsPositive NOTIFY settingsChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY frameChanged)
    Q_PROPERTY(QUrl sourceFolder READ sourceFolder NOTIFY frameChanged)
    Q_PROPERTY(QImage previewImage READ previewImage NOTIFY frameChanged)
    Q_PROPERTY(QImage originalImage READ originalImage NOTIFY frameChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY frameChanged)
    Q_PROPERTY(QVariantList histogram READ histogram NOTIFY frameChanged)
    Q_PROPERTY(QVariantMap latitude READ latitude NOTIFY frameChanged)
    Q_PROPERTY(QVariantList films READ films NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList papers READ papers NOTIFY catalogChanged)
    Q_PROPERTY(int filmIndex READ filmIndex WRITE setFilmIndex NOTIFY settingsChanged)
    Q_PROPERTY(int paperIndex READ paperIndex WRITE setPaperIndex NOTIFY settingsChanged)
    Q_PROPERTY(int decodeMode READ decodeMode WRITE setDecodeMode NOTIFY settingsChanged)
    Q_PROPERTY(double exposureEv READ exposureEv WRITE setExposureEv NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap adjustments READ adjustments NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList libraryItems READ libraryItems NOTIFY libraryChanged)
    Q_PROPERTY(QAbstractItemModel* libraryModel READ libraryModel CONSTANT)
    Q_PROPERTY(int libraryCount READ libraryCount NOTIFY libraryChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY libraryChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY libraryChanged)
    Q_PROPERTY(QImage uncroppedImage READ uncroppedImage NOTIFY frameChanged)
    Q_PROPERTY(QRectF cropRect READ cropRect WRITE setCropRect NOTIFY geometryChanged)
    Q_PROPERTY(bool batchExporting READ batchExporting NOTIFY stateChanged)
    Q_PROPERTY(int batchProgress READ batchProgress NOTIFY stateChanged)
    Q_PROPERTY(int batchTotal READ batchTotal NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QVariantList historyItems READ historyItems NOTIFY historyChanged)
    Q_PROPERTY(int historyIndex READ historyIndex NOTIFY historyChanged)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY historyChanged)
    Q_PROPERTY(QString redoLabel READ redoLabel NOTIFY historyChanged)
    Q_PROPERTY(QString persistenceWarning READ persistenceWarning NOTIFY stateChanged)
    Q_PROPERTY(QString savedStatus READ savedStatus NOTIFY stateChanged)
    Q_PROPERTY(int quarterTurns READ quarterTurns NOTIFY geometryChanged)
    Q_PROPERTY(bool flipHorizontal READ flipHorizontal NOTIFY geometryChanged)
    Q_PROPERTY(bool flipVertical READ flipVertical NOTIFY geometryChanged)
    Q_PROPERTY(double straightenDegrees READ straightenDegrees NOTIFY geometryChanged)
public:
    using ImageFactory=std::function<QImage(const spk::desktop::FramePtr&)>;
    explicit PreviewController(std::filesystem::path resources, QObject* parent=nullptr, ImageFactory imageFactory={}, QString editDirectory={});
    static QImage imageFromFrame(const spk::desktop::FramePtr&);
    ~PreviewController() override;
    bool ready() const { return ready_; }
    bool busy() const { return busy_; }
    bool hasFrame() const { return bool(frame_); }
    bool sourceIsRaw() const { return frame_ && frame_->is_raw; }
    bool failed() const { return failed_; }
    bool dirty() const;
    bool settingsEditable() const { return ready_&&!dialog_open_&&(!busy_||active_operation_=="render"); }
    bool filmIsPositive() const;
    QString status() const { return status_; }
    QString fileName() const;
    QUrl sourceFolder() const;
    QImage previewImage() const { return image_; }
    QImage originalImage() const;
    bool canPaste() const { return bool(clipboard_); }
    QVariantMap metadata() const;
    QVariantList histogram() const;
    QVariantMap latitude() const;
    QVariantList films() const { return films_; }
    QVariantList papers() const { return papers_; }
    int filmIndex() const { return film_index_; }
    int paperIndex() const { return paper_index_; }
    int decodeMode() const { return decode_mode_; }
    double exposureEv() const { return exposure_ev_; }
    QVariantMap adjustments() const;
    QVariantList libraryItems() const;
    QAbstractItemModel* libraryModel(){return &library_model_;}
    int libraryCount()const{return int(library_.size());}
    int activeIndex() const { return active_index_; }
    int selectedCount() const;
    QImage uncroppedImage() const;
    QRectF cropRect() const { return geometry_.crop; }
    int quarterTurns() const { return geometry_.quarterTurns; }
    bool flipHorizontal() const { return geometry_.flipHorizontal; }
    bool flipVertical() const { return geometry_.flipVertical; }
    double straightenDegrees() const { return geometry_.straightenDegrees; }
    bool canUndo() const;
    bool canRedo() const;
    QVariantList historyItems() const;
    int historyIndex() const;
    QString undoLabel() const;
    QString redoLabel() const;
    QString persistenceWarning() const;
    QString savedStatus() const;
    bool batchExporting() const { return busy_ && active_operation_=="batch-export"; }
    int batchProgress() const { return batch_progress_; }
    int batchTotal() const { return batch_total_; }
    void setFilmIndex(int value);
    void setPaperIndex(int value);
    void setDecodeMode(int value);
    void setExposureEv(double value);
    spk::desktop::FramePtr frame() const { return frame_; }
    Q_INVOKABLE void openRaw(const QUrl& path);
    Q_INVOKABLE void openFiles(const QVariantList& urls);
    Q_INVOKABLE void openFolder(const QUrl& folder);
    Q_INVOKABLE void selectImage(int index);
    Q_INVOKABLE void requestThumbnail(int index);
    Q_INVOKABLE bool paperAvailable(int index) const;
    Q_INVOKABLE void setItemSelected(int index, bool selected);
    Q_INVOKABLE void selectAllPhotos(bool selected);
    Q_INVOKABLE void syncSelectedSettings(bool includeCrop=false);
    Q_INVOKABLE void setAdjustment(const QString& name, const QVariant& value);
    Q_INVOKABLE void setCropRect(const QRectF& crop);
    Q_INVOKABLE void resetCrop();
    Q_INVOKABLE void setCropAspect(double ratio);
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void rotateCounterClockwise();
    Q_INVOKABLE void setFlipHorizontal(bool value);
    Q_INVOKABLE void setFlipVertical(bool value);
    Q_INVOKABLE void setStraightenDegrees(double value);
    Q_INVOKABLE void resetGeometry();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void jumpHistory(int index);
    Q_INVOKABLE void beginEditGesture(const QString& key);
    Q_INVOKABLE void endEditGesture();
    Q_INVOKABLE void flushEdits();
    Q_INVOKABLE void apply();
    Q_INVOKABLE void solve();
    Q_INVOKABLE void resetPost();
    Q_INVOKABLE void resetSection(const QString& section);
    Q_INVOKABLE void placeScene(double highlights,double shadows);
    Q_INVOKABLE void setMetering(const QString& method);
    Q_INVOKABLE void copySettings();
    Q_INVOKABLE void pasteSettings(const QVariantList& groups);
    Q_INVOKABLE void exportTiff(const QUrl& path);
    Q_INVOKABLE void exportImage(const QUrl& path, int format, int quality, int maxLongEdge);
    Q_INVOKABLE void exportBatch(const QUrl& folder, int format, int quality, int maxLongEdge);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE void setDialogOpen(bool open);
signals:
    void clipboardChanged();
    void stateChanged();
    void catalogChanged();
    void settingsChanged();
    void frameChanged();
    void sourceOpened();
    void libraryChanged();
    void geometryChanged();
    void historyChanged();
    void initialized(bool success);
    void operationFinished(const QString& operation, bool success);
private:
    struct Reply {
        quint64 generation=0;
        quint64 settings_revision=0;
        QString operation, error;
        QString message;
        int library_index=-1;
        spk::desktop::output::Geometry geometry;
        spk::desktop::FramePtr frame;
        std::optional<spk::desktop::RenderSettings> wanted;
        QImage post_pixels;
        QImage thumbnail;
        QUrl thumbnail_source;
        bool post_only=false;
        spk::desktop::Catalog catalog;
    };
    using Job = std::function<Reply(spk::desktop::PreviewHost&)>;
    static Reply preparePresentation(Reply reply);
    QImage geometryImage(const spk::desktop::output::Geometry& geometry) const;
    void workerLoop(std::filesystem::path resources);
    void deliver(Reply reply);
    void accept(Reply reply);
    void submit(Job job, QString status, QString operation);
    spk::desktop::RenderSettings settings() const;
    void restoreSettings();
    void loadSettings(const spk::desktop::RenderSettings&, spk::desktop::DecodeMode);
    void saveActiveSettings(bool recordHistory=false);
    void setGeometry(const spk::desktop::output::Geometry&,const QString& editKey={});
    spk::desktop::editing::State currentState() const;
    void restoreHistory(bool redo);
    void scheduleSave();
    void updateThumbnail();
    void dispatchThumbnail();
    int addFile(const QUrl&);
    void openItem(int index);
    void changed(const QString& editKey={});
    void reconcileFilmEdge(spk::desktop::RenderSettings& next);
    void schedulePreview();
    bool ready_=false, busy_=true, failed_=false;
    QString status_=QStringLiteral("正在启动渲染引擎…");
    QVariantList films_, papers_;
    spk::desktop::Catalog catalog_;
    int film_index_=0, paper_index_=0, decode_mode_=0;
    double exposure_ev_=0;
    spk::desktop::FramePtr frame_;
    QImage image_;
    QImage full_image_;
    QImage post_pixels_;
    spk::desktop::RenderSettings shown_settings_;
    mutable std::shared_ptr<const spk::io::DecodedRaw> original_source_;
    mutable QImage original_image_,original_view_;
    mutable spk::desktop::output::Geometry original_geometry_;
    std::optional<spk::desktop::editing::State> clipboard_;
    spk::desktop::output::Geometry geometry_;
    spk::desktop::RenderSettings extra_settings_;
    struct LibraryItem {
        QUrl url;
        spk::desktop::RenderSettings settings;
        spk::desktop::DecodeMode mode=spk::desktop::DecodeMode::compatible16;
        spk::desktop::output::Geometry geometry;
        QImage thumbnail;
        bool thumbnail_requested=false,thumbnail_from_camera=false;
        QString error;
        bool selected=true;
        std::vector<spk::desktop::editing::State> undo,redo;
        bool pending_save=false,save_blocked=false,has_stored=false;
        QString save_warning;
    };
    static spk::desktop::editing::State itemState(const LibraryItem&);
    void recordEdit(LibraryItem&,const spk::desktop::editing::State& before);
    std::vector<LibraryItem> library_;
    LibraryModel library_model_;
    std::deque<int> thumbnail_requests_;
    bool thumbnail_running_=false;
    int active_index_=-1;
    std::filesystem::path resources_;
    spk::desktop::editing::Store edit_store_;
    QTimer save_timer_;
    int gesture_index_=-1;
    QString gesture_key_;
    bool gesture_recorded_=false;
    std::vector<spk::desktop::editing::State> gesture_redo_;
    int batch_progress_=0, batch_total_=0;
    std::shared_ptr<std::atomic_bool> cancel_batch_;
    ImageFactory image_factory_;
    quint64 generation_=0;
    quint64 settings_revision_=0;
    QString active_operation_;
    QTimer auto_apply_;
    bool dialog_open_=false;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    bool stopping_=false;
    std::thread worker_;
};
