#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include <QDialog>
#include <QThread>
#include <QImage>
#include <QVector>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QComboBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMap>
#include <opencv2/opencv.hpp>

// ===== 标定项目结构 =====
struct CalibrationProject {
    QString name;           // 项目名称
    QString imageDir;       // 标定图像目录
    QString boardImagePath; // 标定板图像路径
    int boardWidth;         // 棋盘格内角点宽
    int boardHeight;        // 棋盘格内角点高
    double squareSizeMM;    // 方格尺寸(mm)

    CalibrationProject()
        : boardWidth(9)
        , boardHeight(6)
        , squareSizeMM(2.5) {}

    CalibrationProject(const QString& n, const QString& dir,
                       const QString& board, int w, int h, double size)
        : name(n)
        , imageDir(dir)
        , boardImagePath(board)
        , boardWidth(w)
        , boardHeight(h)
        , squareSizeMM(size) {}
};

// ===== 标定结果结构 =====
struct CalibrationResult {
    bool success;
    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;
    double reprojectionError;
    double scale;
    int imageWidth;
    int imageHeight;
    QString errorMessage;

    CalibrationResult()
        : success(false)
        , reprojectionError(0)
        , scale(0)
        , imageWidth(0)
        , imageHeight(0) {}
};

// ===== 标定工作线程 =====
class CalibrationWorker : public QObject
{
    Q_OBJECT

public:
    explicit CalibrationWorker(QObject* parent = nullptr);

    void setParameters(const QString& imageDir,
                       const QString& boardImagePath,
                       int boardWidth,
                       int boardHeight,
                       double squareSizeMM);

public slots:
    void startCalibration();
    void stopCalibration();

signals:
    void progressUpdated(int percent, const QString& message);
    void calibrationComplete(const CalibrationResult& result);
    void errorOccurred(const QString& error);

private:
    bool findChessboardCorners(const cv::Mat& image,
                               cv::Size boardSize,
                               std::vector<cv::Point2f>& corners);
    double calculateScale(const cv::Mat& image,
                          const std::vector<cv::Point2f>& corners,
                          double squareSizeMM);

    QString m_imageDir;
    QString m_boardImagePath;
    int m_boardWidth;
    int m_boardHeight;
    double m_squareSizeMM;
    bool m_stop;
};

// ===== 标定对话框（纯代码生成，无 .ui 文件） =====
class CalibrationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationDialog(QWidget *parent = nullptr);
    ~CalibrationDialog();

    CalibrationResult getResult() const { return m_result; }

    // 添加/管理标定项目
    void addProject(const CalibrationProject& project);
    void addProject(const QString& name, const QString& imageDir,
                    const QString& boardImagePath = "",
                    int boardWidth = 9, int boardHeight = 6,
                    double squareSizeMM = 2.5);
    void removeProject(const QString& name);
    void clearProjects();
    void setCurrentProject(const QString& name);
    QString getCurrentProjectName() const;

private slots:
    void onProjectChanged(int index);
    void onSelectDirClicked();
    void onSelectBoardImageClicked();
    void onAddProjectClicked();        // ← 添加这行
    void onDeleteProjectClicked();
    void onStartClicked();
    void onStopClicked();
    void onSaveClicked();

    void onProgressUpdated(int percent, const QString& message);
    void onCalibrationComplete(const CalibrationResult& result);
    void onErrorOccurred(const QString& error);

private:
    void setupUI();
    void updateUIState(bool calibrating);
    void displayResult(const CalibrationResult& result);
    QString formatMatrix(const cv::Mat& matrix);
    bool saveCalibrationParams(const CalibrationResult& result, const QString& filePath);
    void loadProject(const CalibrationProject& project);
    void updateProjectComboBox();

    // 控件指针
    QComboBox* m_comboProject;
    QPushButton* m_btnAddProject;
    QPushButton* m_btnDeleteProject;
    QLineEdit* m_lineEditImageDir;
    QLineEdit* m_lineEditBoardImage;
    QSpinBox* m_spinBoxBoardWidth;
    QSpinBox* m_spinBoxBoardHeight;
    QDoubleSpinBox* m_doubleSpinBoxSquareSize;
    QProgressBar* m_progressBar;
    QTextEdit* m_textEditLog;
    QTextEdit* m_textEditParams;
    QPushButton* m_btnSelectDir;
    QPushButton* m_btnSelectBoardImage;
    QPushButton* m_btnStart;
    QPushButton* m_btnStop;
    QPushButton* m_btnSave;

    // 项目数据
    QMap<QString, CalibrationProject> m_projects;
    QString m_currentProjectName;

    QThread* m_workerThread;
    CalibrationWorker* m_worker;
    CalibrationResult m_result;
    bool m_isCalibrating;
};

#endif // CALIBRATIONDIALOG_H