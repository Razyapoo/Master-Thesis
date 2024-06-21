#ifndef DATAANALYSISWINDOW_H
#define DATAANALYSISWINDOW_H

#include <QDialog>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QLineSeries>
// #include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QRandomGenerator>
#include <QComboBox>
#include <QPushButton>
// #include <QtCharts/QValueAxis>
#include <QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QDateTimeAxis>
#include <QTimeEdit>
#include <QtWidgets/QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollBar>
#include <QInputDialog>
#include <QThread>

#include "customchartview.h"
#include "custominputdialog.h"
#include "indoorpositioningsystemviewmodel.h"
#include <optional>
#include <vector>
#include <QGridLayout>

namespace Ui {
class DataAnalysisWindow;
}


enum WidgetType {
    DistancesVsTimestamps,
    RollingDeviation,
    datasetSegments
};

class DataAnalysisWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DataAnalysisWindow(QWidget *parent = nullptr, IndoorPositioningSystemViewModel* viewModel = nullptr);
    ~DataAnalysisWindow();

public slots:
    // void onDialogClosed();

    void showAvailableTags(const std::vector<int>& availableTagIDs);
    void showAvailableAnchors(const std::vector<int>& availableAnchorIDs);
    void showPlotDistancesVsTimestamps(const std::vector<long long>& timestamps, std::vector<double*> distances);
    void initDataAnalysis();
    void startDataAnalysis();
    void showPlotRollingDeviations(const std::vector<long long>& timestamps, const std::vector<double>& deviations);
    void validateRollingDeviationInput();
    void validateThresholdInput();
    void showDatasetSegments(const std::vector<double>& datasetSegmentMeans);
    void showOriginalVsAdjustedDistances(const std::vector<long long>& timestampsToAnalyze, std::vector<double*> distancesToAnalyzeOriginal, const std::vector<double>& distancesToAnalyzeAdjusted);
    void uploadReferenceValues();
    // void attachReferenceValues();
    // void calculateLinearRegression();

signals:
    void requestAnalyseData(const long long startTimeSec, const long long endTimeSec);
    void requestCollectDataForTag(int tagID);
    void requestCollectDataForAnchor(const int anchorID);
    void requestCollectDataForPlotDistancesVsTimestamps(const int anchorID);
    void requestCalculateRollingDeviation(const int windowSize);
    void requestSplitDataset(const double threshold);
    void requestCalculatePolynomialRegression(const std::vector<double>& referenceValues);
    void requestUpdateOriginalWithAdjustedValues();
    void requestSegmentFramesExport();
    // void requestUploadReferenceValues(std::vector<double> datasetSegmentMeans);

    // void requestCalculateLinearRegression();



    // void requestPlotDistancesVsTimestamps();

private:
    Ui::DataAnalysisWindow *ui;
    QThread* dataAnalysisWindowThread;
    IndoorPositioningSystemViewModel* viewModel;
    int sizeOfProcessingData;
    int rollingDeviationWindowSize;
    qreal maxStdDeviation;
    std::vector<double> referenceValues;

    QTimeEdit *startAnalysisTime, *endAnalysisTime;
    QPushButton *setAnalysisTimeRangeButton;

    QVBoxLayout* mainLayout;
    QVBoxLayout* chartsLayout;
    QHBoxLayout *tagsAndAnchorsListsLayout;

    QScrollArea *scrollArea;
    QWidget* scrollAreaWidget;
    // QScrollBar *scrollBar;

    QPushButton *analyzeDataButton;
    // QPushButton *addGroundTruthValuesButton;

    QComboBox* comboBoxAvailableTags;
    QComboBox* comboBoxAvailableAnchors;

    // QLineSeries *seriesDistancesVsTimestamps;
    QChart *chartDistancesVsTimestamps;
    // QLineSeries *seriesRollingDeviations;
    CustomChartView *chartViewDistancesVsTimestamps;


    QChart *chartRollingDeviations;
    CustomChartView *chartViewRollingDeviations;
    QHBoxLayout *rollingDeviationInputLayout;
    QLabel *rollingDeviationInputLabel;
    QLineEdit *rollingDeviationInputText;
    QPushButton *calculateRollingDeviationButton;

    CustomChartView *chartViewThresholdInput;
    QHBoxLayout *thresholdInputLayout;
    QLabel *thresholdInputLabel;
    QLineEdit *thresholdInputText;
    QPushButton *thresholdInputButton;

    std::vector<double> segmentMeans;
    QVBoxLayout* segmentMeansLayout;
    QVector<QLabel*> segmentMeansLabels;

    QVector<QPushButton*> adjustReferenceValueButtons;
    QPushButton* uploadReferenceValuesButton;
    QPushButton* calculatePolynomialRegressionButton;

    QChart *chartOriginalVsAdjustedDistances;
    CustomChartView *chartViewOriginalVsAdjustedDistances;

    QPushButton* updateOriginalWithAdjustedValuesButton;
    QPushButton* exportSegementFramesForModelButton;

    void rollingDeviationInit();
    void initThresholdSetting();

    // void chartCleanup();

// protected:
//     void closeEvent(QCloseEvent *event) override;
};

#endif // DATAANALYSISWINDOW_H