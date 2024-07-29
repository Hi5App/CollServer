#ifndef COLL_SERVER_H
#define COLL_SERVER_H

#include <QTimer>
#include <QTcpServer>
#include <set>
#include "collclient.h"
#include "collthread.h"
#include "colldetection.h"

class CollServer:public QTcpServer
{
    Q_OBJECT
public:
    CollServer(QString port,QString project,QString image,QString neuron,QString ananame,QString prefix,int maxUserNumsInt,int modelDetectIntervals,QObject *parent=nullptr);
    virtual ~CollServer();
    void incomingConnection(qintptr handle);

    bool addmarkers(const QString msg, QString comment);
    void delmarkers(const QString msg);
    CollDetection* detectUtil;
//    QFile* additionalLogFile;
//    QTextStream additionalLogOut;

    static CollServer* getInstance();

    QStringList msglist;
    int processedmsgcnt = 0;
    int savedmsgcnt = 0;
    int receivedcnt = 0;

    int removedOverlapSegNum = 0;
    int removedErrSegNum = 0;

    bool isFirstClient = true;
    QMap<QString,CollClient*> hashmap;//user->client
    V_NeuronSWC_list segments;
    V_NeuronSWC_list last1MinSegments;
    V_NeuronSWC_list last3MinSegments;
    V_NeuronSWC_list segmentsForOthersDetect;
    V_NeuronSWC_list segmentsForMissingDetect;

    QList<CellAPO> markers;

    QString swcpath;
    QString apopath;
    QString anopath;

    QString tmp_swcpath;
    QString tmp_apopath;
    QString tmp_anopath;

    string apoName;
    string swcName;

    int ModelDetectIntervals;
    int MaxUserNums;
    int currentUserNum=0;

    string attachmentUuid;
    string swcUuid;

    QMutex mutex;
    QMutex mutexForDetectOthers;
    QMutex mutexForDetectMissing;
    QSemaphore semaphore = QSemaphore(0);
    QString RES;

    bool isSomaExists;
    XYZ somaCoordinate;

    string serverIP;
    string dbmsServerPort;
    string brainServerPort;
    string superuserServerPort;
    string apiVersion;

    CachedProtoData cachedUserData;

signals:
//    void clientAddMarker(QString);
    void clientSendMsgs(QStringList);
    void clientUpdatesendmsgcnt();
    void clientSendmsgs2client(int);
    void clientDeleteLater();
    void clientDisconnectFromHost(CollClient*,bool);
    void imediateSaveDone();
    void overwriteSwcNodeDataDone();

public slots:
    void imediateSave(bool);
    void autoSave();
    void autoExit();
    void updateNParentInfo();
    void overwriteSwcNodeData(bool);

    void RemoveList(QThread* thread);

    void startTimerForDetectLoops();
    void startTimerForDetectOthers();
    void startTimerForDetectTip();
    void startTimerForDetectBranching();
    void startTimerForDetectCrossing();
    void startTimerForDetectWhole();


private:
//    qsizetype idxforprocessed=0;
    QString Port;
    QString Project;
    QString Image;
    QString Neuron;
    QString AnoName;
    QString Prefix;

    QTimer *timerForAutoSave;
    QTimer *timerForDetectLoops;
    QTimer *timerForDetectOthers;
    QTimer *timerForDetectWhole;
    QTimer *timerForDetectTip;
    QTimer *timerForDetectBranching;
    QTimer *timerForDetectCrossing;
    QTimer *timerForAutoExit;
    QTimer *timerForUpdateNParentInfo;

    static CollServer* curServer;
    QList<CollThread*> list_thread;

    QTimer* m_HeartBeatTimer;
    QTimer* m_OnlineStatusTimer;
    QString username="server";
    QString password="123456";

public:
    QString getAnoName();
    QString getProject();
    QString getImage();
    QTimer* getTimerForDetectLoops();
    QTimer* getTimerForDetectOthers();
    QTimer* getTimerForDetectTip();
    QTimer *getTimerForDetectBranching();
    QTimer* getTimerForDetectCrossing();
    QTimer* getTimerForDetectWhole();
    bool connectToDBMS();

};

#endif // COLL_SERVER_H
