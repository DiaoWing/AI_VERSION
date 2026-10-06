#include "calibrationdialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QScrollArea>
#include <QInputDialog>
#include "configmanager.h"

// ============================================================
// CalibrationWorker 实现
// ============================================================

CalibrationWorker::CalibrationWorker(QObject* parent)
    : QObject(parent)
    , m_boardWidth(9)
    , m_boardHeight(6)
    , m_squareSizeMM(2.5)
    , m_stop(false)
{
}

void CalibrationWorker::setParameters(const QString& imageDir,
                                      const QString& boardImagePath,
                                      int boardWidth,
                                      int boardHeight,
                                      double squareSizeMM)
{
    m_imageDir = imageDir;
    m_boardImagePath = boardImagePath;
    m_boardWidth = boardWidth;
    m_boardHeight = boardHeight;
    m_squareSizeMM = squareSizeMM;
}

void CalibrationWorker::startCalibration()
{
    m_stop = false;
    CalibrationResult result;

    emit progressUpdated(0, "开始标定...");

    try {
        QDir dir(m_imageDir);
        QStringList filters;
        filters << "*.jpg" << "*.png" << "*.bmp" << "*.jpeg";
        QStringList imageFiles = dir.entryList(filters, QDir::Files);

        if (imageFiles.isEmpty()) {
            emit errorOccurred("未找到任何图像文件");
            return;
        }

        emit progressUpdated(10, QString("找到 %1 张图像").arg(imageFiles.size()));

        cv::Size boardSize(m_boardWidth, m_boardHeight);
        std::vector<std::vector<cv::Point2f>> imagePoints;
        std::vector<std::vector<cv::Point3f>> objectPoints;
        std::vector<cv::Size> imageSizes;

        std::vector<cv::Point3f> objectCorners;
        for (int i = 0; i < m_boardHeight; i++) {
            for (int j = 0; j < m_boardWidth; j++) {
                objectCorners.push_back(cv::Point3f(j * m_squareSizeMM,
                                                    i * m_squareSizeMM,
                                                    0));
            }
        }

        int processed = 0;
        int total = imageFiles.size();
        std::vector<cv::Mat> images;

        for (const QString& fileName : imageFiles) {
            if (m_stop) {
                emit errorOccurred("标定被用户停止");
                return;
            }

            QString fullPath = m_imageDir + "/" + fileName;
            cv::Mat image = cv::imread(fullPath.toStdString());
            if (image.empty()) {
                qWarning() << "无法读取图像:" << fileName;
                continue;
            }

            cv::Mat gray;
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

            std::vector<cv::Point2f> corners;
            bool found = cv::findChessboardCorners(gray, boardSize, corners,
                                                   cv::CALIB_CB_ADAPTIVE_THRESH |
                                                       cv::CALIB_CB_FILTER_QUADS);

            if (found) {
                cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                                 cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01));

                imagePoints.push_back(corners);
                objectPoints.push_back(objectCorners);
                imageSizes.push_back(image.size());
                images.push_back(image);
                processed++;
            }

            int percent = 10 + (processed * 80 / total);
            emit progressUpdated(percent, QString("处理图像 %1/%2").arg(processed).arg(total));
        }

        if (processed < 3) {
            emit errorOccurred("有效图像太少（至少需要3张），请检查棋盘格是否清晰可见");
            return;
        }

        emit progressUpdated(90, QString("找到 %1 张有效图像").arg(processed));

        cv::Mat cameraMatrix, distCoeffs;
        std::vector<cv::Mat> rvecs, tvecs;

        double reprojectionError = cv::calibrateCamera(objectPoints,
                                                       imagePoints,
                                                       imageSizes[0],
                                                       cameraMatrix,
                                                       distCoeffs,
                                                       rvecs, tvecs);

        double scale = 0;
        if (!m_boardImagePath.isEmpty()) {
            cv::Mat boardImage = cv::imread(m_boardImagePath.toStdString());
            if (!boardImage.empty()) {
                cv::Mat gray;
                cv::cvtColor(boardImage, gray, cv::COLOR_BGR2GRAY);

                std::vector<cv::Point2f> corners;
                bool found = cv::findChessboardCorners(gray, boardSize, corners,
                                                       cv::CALIB_CB_ADAPTIVE_THRESH |
                                                           cv::CALIB_CB_FILTER_QUADS);
                if (found) {
                    scale = calculateScale(boardImage, corners, m_squareSizeMM);
                }
            }
        }

        result.success = true;
        result.cameraMatrix = cameraMatrix.clone();
        result.distCoeffs = distCoeffs.clone();
        result.reprojectionError = reprojectionError;
        result.scale = scale;
        result.imageWidth = imageSizes[0].width;
        result.imageHeight = imageSizes[0].height;

        emit progressUpdated(100, "标定完成！");
        emit calibrationComplete(result);

    } catch (const std::exception& e) {
        emit errorOccurred(QString("标定异常: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("标定发生未知异常");
    }
}

void CalibrationWorker::stopCalibration()
{
    m_stop = true;
}

double CalibrationWorker::calculateScale(const cv::Mat& image,
                                         const std::vector<cv::Point2f>& corners,
                                         double squareSizeMM)
{
    if (corners.size() < 4) return 0;

    cv::Point2f p1 = corners[0];
    cv::Point2f p2 = corners[1];

    double pixelDist = cv::norm(p1 - p2);
    double scale = pixelDist / squareSizeMM;

    qDebug() << "比例尺计算: 像素距离=" << pixelDist
             << ", 实际距离=" << squareSizeMM << "mm"
             << ", 比例尺=" << scale << "像素/mm";

    return scale;
}

bool CalibrationWorker::findChessboardCorners(const cv::Mat& image,
                                              cv::Size boardSize,
                                              std::vector<cv::Point2f>& corners)
{
    return cv::findChessboardCorners(image, boardSize, corners,
                                     cv::CALIB_CB_ADAPTIVE_THRESH |
                                         cv::CALIB_CB_FILTER_QUADS);
}

// ============================================================
// CalibrationDialog 实现（纯代码生成控件）
// ============================================================

CalibrationDialog::CalibrationDialog(QWidget *parent)
    : QDialog(parent)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_isCalibrating(false)
    , m_comboProject(nullptr)
    , m_btnAddProject(nullptr)
    , m_btnDeleteProject(nullptr)
    , m_lineEditImageDir(nullptr)
    , m_lineEditBoardImage(nullptr)
    , m_spinBoxBoardWidth(nullptr)
    , m_spinBoxBoardHeight(nullptr)
    , m_doubleSpinBoxSquareSize(nullptr)
    , m_progressBar(nullptr)
    , m_textEditLog(nullptr)
    , m_textEditParams(nullptr)
    , m_btnSelectDir(nullptr)
    , m_btnSelectBoardImage(nullptr)
    , m_btnStart(nullptr)
    , m_btnStop(nullptr)
    , m_btnSave(nullptr)
{
    setWindowTitle("相机标定");
    setModal(true);
    resize(800, 600);

    setupUI();

    // 添加默认示例项目（用户可在此添加自己的项目）
    // 这里只是示例，实际使用时可以根据需要添加
    addProject("默认项目", "", "", 9, 6, 2.5);
}

CalibrationDialog::~CalibrationDialog()
{
    if (m_worker) {
        m_worker->stopCalibration();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void CalibrationDialog::setupUI()
{
    // ===== 主布局 =====
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ===== 项目选择区域 =====
    QGroupBox* projectGroup = new QGroupBox("标定项目", this);
    QHBoxLayout* projectLayout = new QHBoxLayout(projectGroup);

    projectLayout->addWidget(new QLabel("当前项目:", this));

    m_comboProject = new QComboBox(this);
    m_comboProject->setMinimumWidth(200);
    projectLayout->addWidget(m_comboProject);

    m_btnAddProject = new QPushButton("添加项目", this);
    projectLayout->addWidget(m_btnAddProject);

    m_btnDeleteProject = new QPushButton("删除项目", this);
    projectLayout->addWidget(m_btnDeleteProject);

    projectLayout->addStretch();

    mainLayout->addWidget(projectGroup);

    // ===== 参数设置区域 =====
    QGroupBox* paramGroup = new QGroupBox("标定参数设置", this);
    QGridLayout* paramLayout = new QGridLayout(paramGroup);

    // 第一行：标定图像目录
    paramLayout->addWidget(new QLabel("标定图像目录:", this), 0, 0);
    m_lineEditImageDir = new QLineEdit(this);
    m_lineEditImageDir->setReadOnly(true);
    paramLayout->addWidget(m_lineEditImageDir, 0, 1);
    m_btnSelectDir = new QPushButton("浏览...", this);
    paramLayout->addWidget(m_btnSelectDir, 0, 2);

    // 第二行：标定板图像
    paramLayout->addWidget(new QLabel("标定板图像:", this), 1, 0);
    m_lineEditBoardImage = new QLineEdit(this);
    m_lineEditBoardImage->setReadOnly(true);
    paramLayout->addWidget(m_lineEditBoardImage, 1, 1);
    m_btnSelectBoardImage = new QPushButton("浏览...", this);
    paramLayout->addWidget(m_btnSelectBoardImage, 1, 2);

    // 第三行：棋盘格参数
    paramLayout->addWidget(new QLabel("棋盘格内角点宽:", this), 2, 0);
    m_spinBoxBoardWidth = new QSpinBox(this);
    m_spinBoxBoardWidth->setRange(3, 20);
    m_spinBoxBoardWidth->setValue(9);
    paramLayout->addWidget(m_spinBoxBoardWidth, 2, 1);

    paramLayout->addWidget(new QLabel("高:", this), 2, 2);
    m_spinBoxBoardHeight = new QSpinBox(this);
    m_spinBoxBoardHeight->setRange(3, 20);
    m_spinBoxBoardHeight->setValue(6);
    paramLayout->addWidget(m_spinBoxBoardHeight, 2, 3);

    paramLayout->addWidget(new QLabel("方格尺寸:", this), 2, 4);
    m_doubleSpinBoxSquareSize = new QDoubleSpinBox(this);
    m_doubleSpinBoxSquareSize->setRange(0.1, 100.0);
    m_doubleSpinBoxSquareSize->setValue(2.5);
    m_doubleSpinBoxSquareSize->setSuffix(" mm");
    paramLayout->addWidget(m_doubleSpinBoxSquareSize, 2, 5);

    paramLayout->setColumnStretch(1, 1);
    paramLayout->setColumnStretch(3, 1);

    mainLayout->addWidget(paramGroup);

    // ===== 进度条 =====
    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->addWidget(new QLabel("进度:", this));
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    progressLayout->addWidget(m_progressBar);
    mainLayout->addLayout(progressLayout);

    // ===== 按钮区域 =====
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_btnStart = new QPushButton("开始标定", this);
    m_btnStart->setMinimumWidth(100);
    buttonLayout->addWidget(m_btnStart);

    m_btnStop = new QPushButton("停止", this);
    m_btnStop->setMinimumWidth(80);
    m_btnStop->setEnabled(false);
    buttonLayout->addWidget(m_btnStop);

    m_btnSave = new QPushButton("保存参数", this);
    m_btnSave->setMinimumWidth(100);
    m_btnSave->setEnabled(false);
    buttonLayout->addWidget(m_btnSave);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // ===== 结果显示区域（左右分栏） =====
    QHBoxLayout* resultLayout = new QHBoxLayout();

    // 左侧：参数显示
    QGroupBox* paramDisplayGroup = new QGroupBox("标定参数", this);
    QVBoxLayout* paramDisplayLayout = new QVBoxLayout(paramDisplayGroup);
    m_textEditParams = new QTextEdit(this);
    m_textEditParams->setReadOnly(true);
    m_textEditParams->setFont(QFont("Courier New", 9));
    m_textEditParams->setMinimumHeight(180);
    paramDisplayLayout->addWidget(m_textEditParams);
    resultLayout->addWidget(paramDisplayGroup, 1);

    // 右侧：日志
    QGroupBox* logGroup = new QGroupBox("日志", this);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    m_textEditLog = new QTextEdit(this);
    m_textEditLog->setReadOnly(true);
    m_textEditLog->setFont(QFont("Consolas", 9));
    m_textEditLog->setMinimumHeight(180);
    logLayout->addWidget(m_textEditLog);
    resultLayout->addWidget(logGroup, 1);

    mainLayout->addLayout(resultLayout);

    // ===== 连接信号槽 =====
    connect(m_comboProject, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalibrationDialog::onProjectChanged);
    connect(m_btnAddProject, &QPushButton::clicked,
            this, &CalibrationDialog::onAddProjectClicked);
    connect(m_btnDeleteProject, &QPushButton::clicked,
            this, &CalibrationDialog::onDeleteProjectClicked);
    connect(m_btnSelectDir, &QPushButton::clicked,
            this, &CalibrationDialog::onSelectDirClicked);
    connect(m_btnSelectBoardImage, &QPushButton::clicked,
            this, &CalibrationDialog::onSelectBoardImageClicked);
    connect(m_btnStart, &QPushButton::clicked,
            this, &CalibrationDialog::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked,
            this, &CalibrationDialog::onStopClicked);
    connect(m_btnSave, &QPushButton::clicked,
            this, &CalibrationDialog::onSaveClicked);

    // 初始化UI状态
    updateUIState(false);
}

void CalibrationDialog::addProject(const CalibrationProject& project)
{
    if (m_projects.contains(project.name)) {
        qWarning() << "项目名称已存在:" << project.name;
        return;
    }

    m_projects[project.name] = project;
    updateProjectComboBox();

    // 如果这是第一个项目，自动选中
    if (m_projects.size() == 1) {
        m_comboProject->setCurrentIndex(0);
        loadProject(project);
    }
}

void CalibrationDialog::addProject(const QString& name, const QString& imageDir,
                                   const QString& boardImagePath,
                                   int boardWidth, int boardHeight,
                                   double squareSizeMM)
{
    CalibrationProject project(name, imageDir, boardImagePath,
                               boardWidth, boardHeight, squareSizeMM);
    addProject(project);
}

void CalibrationDialog::removeProject(const QString& name)
{
    if (name == "默认项目" && m_projects.size() == 1) {
        QMessageBox::warning(this, "提示", "不能删除最后一个项目");
        return;
    }

    if (m_projects.contains(name)) {
        m_projects.remove(name);
        updateProjectComboBox();

        if (!m_projects.isEmpty()) {
            m_comboProject->setCurrentIndex(0);
            loadProject(m_projects.value(m_comboProject->currentText()));
        } else {
            // 清空界面
            m_lineEditImageDir->clear();
            m_lineEditBoardImage->clear();
            m_currentProjectName.clear();
        }
    }
}

void CalibrationDialog::clearProjects()
{
    m_projects.clear();
    m_comboProject->clear();
    m_currentProjectName.clear();

    m_lineEditImageDir->clear();
    m_lineEditBoardImage->clear();
    m_textEditLog->clear();
    m_textEditParams->clear();
    m_progressBar->setValue(0);
}

void CalibrationDialog::setCurrentProject(const QString& name)
{
    if (m_projects.contains(name)) {
        int index = m_comboProject->findText(name);
        if (index >= 0) {
            m_comboProject->setCurrentIndex(index);
            loadProject(m_projects[name]);
        }
    }
}

QString CalibrationDialog::getCurrentProjectName() const
{
    return m_currentProjectName;
}

void CalibrationDialog::updateProjectComboBox()
{
    m_comboProject->clear();
    for (auto it = m_projects.begin(); it != m_projects.end(); ++it) {
        m_comboProject->addItem(it.key());
    }
}

void CalibrationDialog::loadProject(const CalibrationProject& project)
{
    m_currentProjectName = project.name;
    m_lineEditImageDir->setText(project.imageDir);
    m_lineEditBoardImage->setText(project.boardImagePath);
    m_spinBoxBoardWidth->setValue(project.boardWidth);
    m_spinBoxBoardHeight->setValue(project.boardHeight);
    m_doubleSpinBoxSquareSize->setValue(project.squareSizeMM);

    // 清空之前的结果
    m_textEditParams->clear();
    m_result = CalibrationResult();
    m_btnSave->setEnabled(false);

    m_textEditLog->append(QString("切换到项目: %1").arg(project.name));
}

void CalibrationDialog::onProjectChanged(int index)
{
    if (index < 0 || index >= m_comboProject->count()) return;

    QString projectName = m_comboProject->currentText();
    if (m_projects.contains(projectName)) {
        loadProject(m_projects[projectName]);
    }
}

void CalibrationDialog::onAddProjectClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "添加项目",
                                         "请输入项目名称:",
                                         QLineEdit::Normal,
                                         QString("项目_%1").arg(m_projects.size() + 1),
                                         &ok);
    if (!ok || name.isEmpty()) return;

    if (m_projects.contains(name)) {
        QMessageBox::warning(this, "提示", "项目名称已存在");
        return;
    }

    // 创建新项目，复制当前项目的参数作为模板
    CalibrationProject project;
    project.name = name;
    if (!m_currentProjectName.isEmpty() && m_projects.contains(m_currentProjectName)) {
        const CalibrationProject& current = m_projects[m_currentProjectName];
        project.boardWidth = current.boardWidth;
        project.boardHeight = current.boardHeight;
        project.squareSizeMM = current.squareSizeMM;
        project.imageDir = current.imageDir;
        project.boardImagePath = current.boardImagePath;
    } else {
        project.boardWidth = 9;
        project.boardHeight = 6;
        project.squareSizeMM = 2.5;
    }

    m_projects[name] = project;
    updateProjectComboBox();

    // 选中新项目
    int index = m_comboProject->findText(name);
    if (index >= 0) {
        m_comboProject->setCurrentIndex(index);
        loadProject(project);
    }

    m_textEditLog->append(QString("添加项目: %1").arg(name));
}

void CalibrationDialog::onDeleteProjectClicked()
{
    if (m_comboProject->count() == 0) return;

    QString name = m_comboProject->currentText();

    if (name == "默认项目" && m_projects.size() == 1) {
        QMessageBox::warning(this, "提示", "不能删除最后一个项目");
        return;
    }

    int ret = QMessageBox::question(this, "确认删除",
                                    QString("确定要删除项目 \"%1\" 吗？").arg(name),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        removeProject(name);
        m_textEditLog->append(QString("删除项目: %1").arg(name));
    }
}

void CalibrationDialog::onSelectDirClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "选择标定图像目录",
                                                    "",
                                                    QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty()) {
        m_lineEditImageDir->setText(dir);

        // 更新当前项目
        if (!m_currentProjectName.isEmpty() && m_projects.contains(m_currentProjectName)) {
            m_projects[m_currentProjectName].imageDir = dir;
            // 更新 ComboBox 显示（如果需要显示额外信息）
        }

        m_textEditLog->append("选择标定目录: " + dir);
    }
}

void CalibrationDialog::onSelectBoardImageClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "选择标定板图像",
                                                    "",
                                                    "Images (*.jpg *.png *.bmp *.jpeg)");
    if (!filePath.isEmpty()) {
        m_lineEditBoardImage->setText(filePath);

        // 更新当前项目
        if (!m_currentProjectName.isEmpty() && m_projects.contains(m_currentProjectName)) {
            m_projects[m_currentProjectName].boardImagePath = filePath;
        }

        m_textEditLog->append("选择标定板图像: " + filePath);
    }
}

void CalibrationDialog::onStartClicked()
{
    QString imageDir = m_lineEditImageDir->text();
    if (imageDir.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择标定图像目录");
        return;
    }

    QDir dir(imageDir);
    if (!dir.exists()) {
        QMessageBox::warning(this, "提示", "标定图像目录不存在");
        return;
    }

    // 保存当前项目设置
    if (!m_currentProjectName.isEmpty() && m_projects.contains(m_currentProjectName)) {
        m_projects[m_currentProjectName].imageDir = m_lineEditImageDir->text();
        m_projects[m_currentProjectName].boardImagePath = m_lineEditBoardImage->text();
        m_projects[m_currentProjectName].boardWidth = m_spinBoxBoardWidth->value();
        m_projects[m_currentProjectName].boardHeight = m_spinBoxBoardHeight->value();
        m_projects[m_currentProjectName].squareSizeMM = m_doubleSpinBoxSquareSize->value();
    }

    int boardWidth = m_spinBoxBoardWidth->value();
    int boardHeight = m_spinBoxBoardHeight->value();
    double squareSize = m_doubleSpinBoxSquareSize->value();
    QString boardImagePath = m_lineEditBoardImage->text();

    m_workerThread = new QThread(this);
    m_worker = new CalibrationWorker();
    m_worker->moveToThread(m_workerThread);

    m_worker->setParameters(imageDir, boardImagePath, boardWidth, boardHeight, squareSize);

    connect(m_workerThread, &QThread::started, m_worker, &CalibrationWorker::startCalibration);
    connect(m_worker, &CalibrationWorker::progressUpdated,
            this, &CalibrationDialog::onProgressUpdated);
    connect(m_worker, &CalibrationWorker::calibrationComplete,
            this, &CalibrationDialog::onCalibrationComplete);
    connect(m_worker, &CalibrationWorker::errorOccurred,
            this, &CalibrationDialog::onErrorOccurred);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_textEditLog->clear();
    m_textEditParams->clear();
    m_result = CalibrationResult();

    updateUIState(true);
    m_progressBar->setValue(0);
    m_textEditLog->append(QString("开始标定 - 项目: %1").arg(m_currentProjectName));

    m_workerThread->start();
}

void CalibrationDialog::onStopClicked()
{
    if (m_worker) {
        m_worker->stopCalibration();
        m_textEditLog->append("正在停止标定...");
    }
}

void CalibrationDialog::onSaveClicked()
{
    if (!m_result.success) {
        QMessageBox::warning(this, "提示", "没有可保存的标定结果");
        return;
    }

    QString defaultName = QString("calibration_%1.json").arg(m_currentProjectName);
    QString filePath = QFileDialog::getSaveFileName(this,
                                                    "保存标定参数",
                                                    defaultName,
                                                    "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        if (saveCalibrationParams(m_result, filePath)) {
            QMessageBox::information(this, "成功", "标定参数已保存");
        } else {
            QMessageBox::warning(this, "错误", "保存标定参数失败");
        }
    }
}

void CalibrationDialog::onProgressUpdated(int percent, const QString& message)
{
    m_progressBar->setValue(percent);
    m_textEditLog->append(message);
}

void CalibrationDialog::onCalibrationComplete(const CalibrationResult& result)
{
    m_result = result;
    updateUIState(false);
    displayResult(result);
    m_textEditLog->append("标定完成！");

    if (result.success) {
        m_textEditLog->append(QString("重投影误差: %1 像素").arg(result.reprojectionError, 0, 'f', 4));
        if (result.scale > 0) {
            m_textEditLog->append(QString("比例尺: %1 像素/mm").arg(result.scale, 0, 'f', 2));
        }
        m_btnSave->setEnabled(true);

        // ===== 使用 ConfigManager 保存标定结果 =====
        ConfigManager* configMgr = ConfigManager::instance();

        // 获取当前项目的棋盘格参数
        int boardWidth = m_spinBoxBoardWidth->value();
        int boardHeight = m_spinBoxBoardHeight->value();
        double squareSize = m_doubleSpinBoxSquareSize->value();

        // 直接保存，使用单例内部的配置文件路径
        bool saved = configMgr->SaveCalibrationResultTojsonFile(
            result,
            m_currentProjectName,
            boardWidth,
            boardHeight,
            squareSize
            );

        if (saved) {
            m_textEditLog->append("✅ 标定结果已自动保存到配置文件");
        } else {
            m_textEditLog->append("⚠️ 标定结果保存失败");
        }
    }
}

void CalibrationDialog::onErrorOccurred(const QString& error)
{
    updateUIState(false);
    m_textEditLog->append("错误: " + error);
    QMessageBox::critical(this, "标定失败", error);
}

void CalibrationDialog::updateUIState(bool calibrating)
{
    m_isCalibrating = calibrating;

    m_btnStart->setEnabled(!calibrating);
    m_btnStop->setEnabled(calibrating);
    m_btnSelectDir->setEnabled(!calibrating);
    m_btnSelectBoardImage->setEnabled(!calibrating);
    m_spinBoxBoardWidth->setEnabled(!calibrating);
    m_spinBoxBoardHeight->setEnabled(!calibrating);
    m_doubleSpinBoxSquareSize->setEnabled(!calibrating);
    m_lineEditImageDir->setEnabled(!calibrating);
    m_lineEditBoardImage->setEnabled(!calibrating);
    m_comboProject->setEnabled(!calibrating);
    m_btnAddProject->setEnabled(!calibrating);
    m_btnDeleteProject->setEnabled(!calibrating);
}

void CalibrationDialog::displayResult(const CalibrationResult& result)
{
    if (!result.success) {
        m_textEditParams->setText("标定失败");
        return;
    }

    QString text;
    text += "========== 标定结果 ==========\n\n";
    text += QString("项目: %1\n").arg(m_currentProjectName);
    text += QString("图像尺寸: %1 x %2\n").arg(result.imageWidth).arg(result.imageHeight);
    text += QString("重投影误差: %1 像素\n\n").arg(result.reprojectionError, 0, 'f', 4);

    text += "相机内参矩阵:\n";
    text += formatMatrix(result.cameraMatrix) + "\n";

    text += "畸变系数:\n";
    text += formatMatrix(result.distCoeffs) + "\n";

    if (result.scale > 0) {
        text += QString("比例尺: %1 像素/mm\n").arg(result.scale, 0, 'f', 2);
        text += QString("(1mm = %1 像素)\n").arg(result.scale, 0, 'f', 2);
        text += QString("(1像素 = %1 mm)\n").arg(1.0 / result.scale, 0, 'f', 4);
    }

    double fx = result.cameraMatrix.at<double>(0, 0);
    double fy = result.cameraMatrix.at<double>(1, 1);
    text += QString("\n焦距: fx=%1, fy=%2").arg(fx, 0, 'f', 2).arg(fy, 0, 'f', 2);

    double cx = result.cameraMatrix.at<double>(0, 2);
    double cy = result.cameraMatrix.at<double>(1, 2);
    text += QString("\n主点: cx=%1, cy=%2").arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2);

    m_textEditParams->setText(text);
}

QString CalibrationDialog::formatMatrix(const cv::Mat& matrix)
{
    QString text;
    for (int i = 0; i < matrix.rows; i++) {
        text += "[";
        for (int j = 0; j < matrix.cols; j++) {
            text += QString("%1").arg(matrix.at<double>(i, j), 0, 'f', 4);
            if (j < matrix.cols - 1) text += ", ";
        }
        text += "]\n";
    }
    return text;
}

bool CalibrationDialog::saveCalibrationParams(const CalibrationResult& result,
                                              const QString& filePath)
{
    try {
        QJsonObject root;
        root["version"] = "1.0";
        root["projectName"] = m_currentProjectName;
        root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["imageWidth"] = result.imageWidth;
        root["imageHeight"] = result.imageHeight;
        root["reprojectionError"] = result.reprojectionError;
        root["scale"] = result.scale;

        // 保存项目参数
        if (!m_currentProjectName.isEmpty() && m_projects.contains(m_currentProjectName)) {
            const CalibrationProject& project = m_projects[m_currentProjectName];
            root["boardWidth"] = project.boardWidth;
            root["boardHeight"] = project.boardHeight;
            root["squareSizeMM"] = project.squareSizeMM;
        }

        QJsonArray cameraMatrixArr;
        for (int i = 0; i < result.cameraMatrix.rows; i++) {
            for (int j = 0; j < result.cameraMatrix.cols; j++) {
                cameraMatrixArr.append(result.cameraMatrix.at<double>(i, j));
            }
        }
        root["cameraMatrix"] = cameraMatrixArr;

        QJsonArray distCoeffsArr;
        for (int i = 0; i < result.distCoeffs.cols; i++) {
            distCoeffsArr.append(result.distCoeffs.at<double>(0, i));
        }
        root["distCoeffs"] = distCoeffsArr;

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }

        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        return true;

    } catch (...) {
        return false;
    }
}


