#ifndef COLLDETECTION_H
#define COLLDETECTION_H
#include <vector>
#include <set>
#include <unordered_set>
#include "utils.h"
#include "basic_c_fun/basic_surf_objs.h"
#include "neuron_editing/neuron_format_converter.h"
#include <QNetworkReply>
#include "json.hpp"
#include <QJsonArray>

class CollServer;
class CollDetection : public QObject
{
    Q_OBJECT
private:
    CollServer* myServer;
    QNetworkAccessManager* accessManager;
    QString SuperUserHostAddress;
    QString BrainTellHostAddress;
    QString neuronFiberSegUrl;
    QString tipDirPath;
    QString segDirPath;
    vector<NeuronSWC> tipPoints;

public:
    struct TipCoorPredictedResult{
        QString storeDirName;
        XYZ maxResCoor;
        int y_pred;
        TipCoorPredictedResult(){storeDirName=""; maxResCoor=XYZ(); y_pred=-1;}
    };

    struct MissingForSegData{
        XYZ maxResCoor;
        QString storeDirName;
        XYZ centerCoor;
        XYZ edgeCoor;
        int type;
        V_NeuronSWC addedSeg;
        MissingForSegData(){maxResCoor=XYZ(); storeDirName=""; centerCoor=XYZ(); edgeCoor=XYZ();}
    };

    struct CrossingInfo{
        pair<pair<QString, int>, pair<QString, int>> coor2SegIndexPair;
        pair<pair<QString, pair<int, int>>, pair<QString, pair<int, int>>> coor2RowIndexRangePair;
        pair<pair<QString, vector<XYZ>>, pair<QString, vector<XYZ>>> fiberCoorInfoPair;
        bool isAbleCorrect = true;
    };

    struct FiberCoorData{
        int segID;
        pair<int, int> rangePair;
        vector<XYZ> coorVec;
        V_NeuronSWC addedSeg;
    };

    map<QString, MissingForSegData> tipInfoMap;
    map<QString, CrossingInfo> crossingInfoMap;
    bool isAutoCorrect = true;

    static XYZ maxRes;
    static XYZ subMaxRes;
    QTimer *timerForFilterTip;
    unordered_set<string> detectedTipPoints;
    unordered_set<string> detectedBranchingPoints;
    set<set<string>> detectedCrossingPoints;

    explicit CollDetection(CollServer* curServer, string brainServerIP, string superuserServerIP, string neuronfiberSegServerIP, string brainServerPort, string superuserServerPort, string neuronfiberSegServerPort, QObject* parent=nullptr);
    ~CollDetection(){}
    XYZ getSomaCoordinate(QString apoPath);
    vector<NeuronSWC> specStructsDetection(V_NeuronSWC_list& inputSegList, double dist_thresh=0.2);
    vector<NeuronSWC> loopDetection(V_NeuronSWC_list& inputSegList, double dist_thresh=8);
    vector<NeuronSWC> tipDetection(V_NeuronSWC_list inputSegList, bool removeFlag, map<string, set<size_t>> allPoint2SegIdMap, double dist_thresh=30);
    QJsonArray crossingDetection();
    vector<NeuronSWC> branchingDetection(V_NeuronSWC_list inputSegList, double dist_thresh=12);
    void handleMulFurcation(vector<NeuronSWC>& outputSpecialPoints, int& count, double dist_thre=8);
    void handleLoop(vector<NeuronSWC>& outputSpecialPoints, int& count);
    void handleNearBifurcation(vector<NeuronSWC>& bifurPoints, int& count);
    void handleTip(vector<NeuronSWC>& tipPoints);
    void filterTip(vector<NeuronSWC>& markpoints);
    void fliterTip(map<QString, MissingForSegData>& segDataMap);
    void fliterCrossing();

    vector<TipCoorPredictedResult> getMissingPart(vector<TipCoorPredictedResult> tipCoorResults);
    bool requestForSeg(QString relPath, vector<QString> coorList, QString& result_relpath);
    void getApp2TracingResult(map<QString, MissingForSegData>& segDataMap, QString relpath);
    vector<V_NeuronSWC> convertLocal2Global(V_NeuronSWC_list& inputSegList, float x_ratio, float y_ratio, float z_ratio, XYZ startCoor);
    XYZ convertLocal2Global(XYZ inputCoor, float x_ratio, float y_ratio, float z_ratio, XYZ startCoor);
    V_NeuronSWC extractAddedSeg(vector<V_NeuronSWC>& maxResSegVec, XYZ edgeCoorMaxResGlobal, XYZ centerCoorMaxResGlobal);
    void autoCorrectMissing(map<QString, MissingForSegData>& segDataMap);
    void updateCrossingInfoMap(QJsonArray infos);
    vector<V_NeuronSWC> getCrossingCorrectedSegs();
    void autoCorrectCrossing(vector<V_NeuronSWC> addedSegs);

    void handleBranchingPoints(vector<NeuronSWC>& brainchingPoints, int& count);
    void handleCrossing(QJsonArray& json);

    void sortSWC(QString fileOpenName, QString fileSaveName, double thres=1000000000, V3DLONG rootid=1000000000);
    void setSWCRadius(QString filePath, int r);
    void getImageRES();
    void getImageMaxRES();
    void getApoForCrop(QString fileSaveName, vector<NeuronSWC> tipPoints);
    void removeShortSegs(V_NeuronSWC_list inputSegList, double dist_thre=8);
    void removeOverlapSegs(V_NeuronSWC_list inputSegList);

signals:
    void removeErrorSegsDone();
    void tuneErrorSegsDone();

public slots:
    void detectWholeAtStart();
    void detectOthers();
    void detectLoops();
    void detectTips();
    void detectTipsWhole();
    void detectBranchingPoints();
    void detectCrossings();
    void detectOthersWhole();
    void removeErrorSegs(bool);
    void tuneErrorSegs(bool);
};

#endif // COLLDETECTION_H
