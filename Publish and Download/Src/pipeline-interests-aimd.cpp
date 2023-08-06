#include "consumer.hpp"
#include "pipeline-interests-aimd.hpp"
#include "filedlg1.h"

#include <cmath>

namespace ndn {
namespace chunks {
namespace aimd {

PipelineInterestsAimd::PipelineInterestsAimd(Face& face, RttEstimator& rttEstimator,
                                             const Options& options)
  : PipelineInterests(face)
  , m_options(options)
  , m_rttEstimator(rttEstimator)
  , m_scheduler(m_face.getIoService())
  , BbrScheduler(m_face.getIoService())
  , m_checkRtoEvent(m_scheduler)
  , BbrEvent(BbrScheduler)
  , m_nextSegmentNo(0)
  , m_receivedSize(0)
  , m_highData(0)
  , m_highInterest(0)
  , m_recPoint(0)
  , m_nInFlight(0)
  , m_nReceived(0)
  , m_nLossEvents(0)
  , m_nRetransmitted(0)
  , m_cwnd(m_options.initCwnd)
  , m_ssthresh(m_options.initSsthresh)
  , m_hasFailure(false)
  , m_failedSegNo(0)
  , m_log(outfile)
  , SentInterestHighNo(0)
  , LastRtoInFlightsegment(0)
  , RttUpdateFlag(false)
  , DeliveredDataBbrInterval(0)
  , SentDataBbrInterval(0)
  , TimeoutCountBbrInterval(0)
  , BbrState(0)
  , DeliveryRateQueueCount(0)
  , BbrBwSampleGainCount(0)
  , Cwnd_Temp(0)
  , DeliveryRateSample(0)
  , RttValueQueueCount(0)
  , BbrBwSample(0)
  , DeliveredRateTemp(0)
  , DeliveredData(0)
  , LossData(0)
  , SentData(0)
  , SegmentDeliveryRate(0)
  , SegmentDeliveryRateCount(0)
  , SegmentDeliveredRateTemp(0)
  , SegmentDeliveryRateLast(0)
{
  if (m_options.isVerbose) {
    std::cerr << m_options;
  }

  time.start();

  BbrCheckIntervalLast=time::steady_clock::now();

  QDateTime DateTime;
  QString Timestr=DateTime.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");   //设置显示格式
  m_log<<"Time:"<<string((const char *)Timestr.toLocal8Bit())<<"\n";
  m_log<<"Test Name:DQN Test"<<"\n";
  m_log<<"Topology:local"<<"\n\n";
  m_log<<"Data Log"<<"\n";

  m_log<<"cwnd"<<"     ";

  m_log<<"m_nInFlight"<<"     ";
  m_log<<"DeliveredData"<<"     ";
  m_log<<"LossData"<<"     ";   //punish
  m_log<<"RTT"<<"     ";
  m_log<<"DeliveryRate"<<"\n";   //rwd
}

PipelineInterestsAimd::~PipelineInterestsAimd()
{
  cancel();
}

void
PipelineInterestsAimd::doRun()
{
  // count the excluded segment
  m_nReceived++;   //需要排除的分块数（即已经接收的）

  //安排检查转发计时器的事件，一定时间之后的事件
  // schedule the event to check retransmission timer
  m_checkRtoEvent = m_scheduler.scheduleEvent(m_options.rtoCheckInterval, [this] { checkRto(); });


  if (RttUpdateFlag==false)
      BbrEvent=BbrScheduler.scheduleEvent(BbrCheckIntervalInitial,[this]{CheckBbr();});
  else
      BbrEvent=BbrScheduler.scheduleEvent(BbrCheckInterval,[this]{CheckBbr();});



  schedulePackets();   //安排下一个请求
}

void
PipelineInterestsAimd::doCancel()   //取消数据请求
{
  for (const auto& entry : m_segmentInfo) {
    m_face.removePendingInterest(entry.second.interestId);
  }
  m_checkRtoEvent.cancel();
  m_segmentInfo.clear();
}

void
PipelineInterestsAimd::CheckBbr()
{
    BbrCheckIntervalexpense=time::steady_clock::now()-BbrCheckIntervalLast;
    BbrCheckIntervalexpense_ms=boost::chrono::duration_cast<boost::chrono::milliseconds>(BbrCheckIntervalexpense);

    //m_log<<"DeliveredData:"<<DeliveredDataBbrInterval<<"   BbrCheckIntervalexpense_ms:"<<BbrCheckIntervalexpense_ms<<"\n";   //投递率= 除以上一个事件设定的RTT
    //m_log<<"DeliveredData/m_cwnd:"<<double(DeliveredDataBbrInterval)/double(m_cwnd)<<"\n";
    //m_log<<"DeliveredData/BbrCheckIntervalexpense_ms:"<<double(DeliveredDataBbrInterval)/double(BbrCheckIntervalexpense_ms.count())<<"\n";



    //double DeliveryRate=double(DeliveredDataBbrInterval)/double(m_cwnd)/double(BbrCheckIntervalexpense_ms.count());
    double DeliveryRate=double(std::min(SentDataBbrInterval,DeliveredDataBbrInterval))/double(BbrCheckIntervalexpense_ms.count());
    //double DeliveryRate=DeliveredDataBbrInterval;

    //m_log<<DeliveryRate<<"   "<<m_cwnd<<endl;


/*
   m_log<<SegmentDeliveryRate<<"   "<<m_cwnd<<"\n";

   if (BbrState==5)
   {
       if (SegmentDeliveryRateCount==0)
       {
           Cwnd_Temp=SegmentDeliveryRateLast;
           SegmentDeliveredRateTemp=SegmentDeliveryRate;   //?
       }

       SegmentDeliveryRateCount++;

       if (SegmentDeliveryRateCount>=RttValue.count())
       {
           SegmentDeliveryRateCount=0;

           m_cwnd=Cwnd_Temp*(SegmentDeliveryRate/SegmentDeliveredRateTemp);


           if (m_cwnd>Cwnd_Temp+180)
           {
               m_cwnd=Cwnd_Temp+180;
           }
           else if (m_cwnd<Cwnd_Temp-100)
           {
               m_cwnd=Cwnd_Temp-100;
           }
       }
   }

   //m_log<<DeliveryRate<<"   "<<m_cwnd<<endl;

   if (BbrState==0&&m_cwnd>=600)
   {
       BbrState=5;

       SegmentDeliveryRateLast=(m_cwnd*3.0)/4.0;
       m_cwnd=SegmentDeliveryRateLast;
   }

*/

    //outfile.close();

    if (BbrState==0)
    {
        if (DeliveryRateQueueCount<=3)
        {
            DeliveryRateQueue[DeliveryRateQueueCount]=DeliveryRate;
            DeliveryRateQueueCount++;
            if (DeliveryRateQueueCount==4)
            {
                //if (DeliveryRateQueue[0]!=0&&DeliveryRateQueue[1]!=0&&DeliveryRateQueue[2]!=0&&DeliveryRateQueue[3]!=0)
                //{
                    if (DeliveryRateQueue[1]<DeliveryRateQueue[0]*1.25&&DeliveryRateQueue[2]<DeliveryRateQueue[1]*1.25&&DeliveryRateQueue[3]<DeliveryRateQueue[2]*1.25)
                    {
   //                     BbrState=1;
                    }
                //}
            }
        }
        else
        {
            DeliveryRateQueue[4]=DeliveryRateQueue[0];   //
            DeliveryRateQueue[0]=DeliveryRateQueue[1];
            DeliveryRateQueue[1]=DeliveryRateQueue[2];
            DeliveryRateQueue[2]=DeliveryRateQueue[3];
            DeliveryRateQueue[3]=DeliveryRate;

            //if (DeliveryRateQueue[0]!=0&&DeliveryRateQueue[1]!=0&&DeliveryRateQueue[2]!=0&&DeliveryRateQueue[3]!=0)
            //{
                if (DeliveryRateQueue[1]<DeliveryRateQueue[0]*1.25&&DeliveryRateQueue[2]<DeliveryRateQueue[1]*1.25&&DeliveryRateQueue[3]<DeliveryRateQueue[2]*1.25)
                {
//                    BbrState=1;   //1?
                    //m_log<<"333333333333333333333"<<"\n";
                }
            //}
        }
        if (BbrState==1)
        {
            m_cwnd=m_cwnd/3;   //?
            //m_cwndSampleFirst=m_cwnd;
        }
    }
    else if (BbrState==1&&m_nInFlight<=m_cwnd)   //排空
    {
        BbrState=2;   //2?
    }
    else if (BbrState==2)   //排空之后交给探测   之后交给排空hun huan?
    {
        if (BbrBwSampleGainCount==0)   //不需要BbrState=1来等待排空？因为它必须是8个周期完成
        {
            Cwnd_Temp=m_cwnd;
            DeliveredRateTemp=DeliveryRate;   //?

            m_cwnd=m_cwnd*BbrBwSampleGain[BbrBwSampleGainCount];
            BbrBwSampleGainCount++;

            //BbrState==1;   //?
            //?
            //BbrCheckInterval=BbrCheckInterval*2;

            //BbrCheckInterval=BbrCheckInterval.operator *=(5);
            //BbrCheckInterval=BbrCheckInterval.operator /=(4);

            //BbrCheckInterval=RttValue*2;   //?

            //m_log<<"m_rttEstimator.getEstimatedRto():"<<m_rttEstimator.getEstimatedRto()<<"\n";
            //BbrCheckInterval=boost::chrono::duration_cast<boost::chrono::milliseconds>(m_rttEstimator.getEstimatedRto());
//            BbrCheckInterval=min(RttValue*5/4,boost::chrono::duration_cast<boost::chrono::milliseconds>(m_rttEstimator.getEstimatedRto()));
            //BbrCheckInterval=boost::chrono::duration_cast<boost::chrono::milliseconds>(m_rttEstimator.getEstimatedRto());
        }
        else if (BbrBwSampleGainCount==1)
        {//BbrCheckInterval=200;
//            time::milliseconds BbrCheckInterval_{200};
//            BbrCheckInterval=BbrCheckInterval_;
            DeliveryRateSample=DeliveryRate;
            //DeliveryRateSample=DeliveredDataBbrInterval;   //

            BbrCheckIntervalexpense_msTemp=BbrCheckIntervalexpense_ms;
            m_cwnd=Cwnd_Temp*BbrBwSampleGain[BbrBwSampleGainCount];
            BbrBwSampleGainCount++;
        }
        else
        {
            //m_cwnd=DeliveryRateSample*Cwnd_Temp*boost::chrono::duration_cast<boost::chrono::milliseconds>(BbrCheckInterval).count();

            //m_cwnd=DeliveryRateSample*Cwnd_Temp*1.25*double(BbrCheckIntervalexpense_msTemp.count());
            //m_cwnd=Cwnd_Temp+(DeliveryRateSample-DeliveredDataTemp);

            //m_cwnd=Cwnd_Temp*1.25*((DeliveryRateSample+1)/(DeliveredRateTemp+1));

            if (DeliveryRateSample/double(BbrCheckIntervalexpense_msTemp.count())*200>Cwnd_Temp*5/4)
            {
                m_cwnd=Cwnd_Temp*5/4;
            }
            else if (DeliveryRateSample/double(BbrCheckIntervalexpense_msTemp.count())*200<Cwnd_Temp*3/4)
            {
                m_cwnd=Cwnd_Temp*3/4;
            }
            else
            {
                m_cwnd=DeliveryRateSample/double(BbrCheckIntervalexpense_msTemp.count())*200;
            }

            BbrBwSampleGainCount++;
            if (BbrBwSampleGainCount==8&&m_nInFlight>m_cwnd)
            {
                BbrState=1;   //pai kong
            }

        }
        if (BbrBwSampleGainCount>=8)   //8?
        {
            BbrBwSampleGainCount=0;
        }
        if (m_cwnd<4)   //4?
        {
            m_cwnd=4;
        }
    }


//m_log<<"BbrState:BbrState:BbrState:BbrState:"<<BbrState<<"\n";
//m_log<<"DeliveryRateQueue:"<<DeliveryRateQueue[0]<<"   "<<DeliveryRateQueue[1]<<"   "<<DeliveryRateQueue[2]<<"   "<<DeliveryRateQueue[3]<<"\n";

    DeliveredDataBbrInterval=0;
    SentDataBbrInterval=0;
    BbrCheckIntervalLast=time::steady_clock::now();

    //BbrCheckInterval=BbrCheckInterval+0.5*
    //BbrCheckInterval=boost::chrono::duration_cast<boost::chrono::milliseconds>(m_rttEstimator.getEstimatedRto());


    if (RttUpdateFlag==false)
    {
        BbrEvent=BbrScheduler.scheduleEvent(BbrCheckIntervalInitial,[this]{CheckBbr();});
    }
    else
    {
        BbrEvent=BbrScheduler.scheduleEvent(BbrCheckInterval,[this]{CheckBbr();});
    }

    schedulePackets();
}

void   //简单的拥塞判断
PipelineInterestsAimd::checkRto()   //检查Rto(超时重传)中所有发出但还未应答的请求（每次检查完，设定一定时间间隔的下一次检查）
{
  if (Filedlg1::DownLoadStatus==2)   //下载停止状态
    return;

  //Milliseconds rtt = time::steady_clock::now() - timebeginlast;
  //m_log<<"CheckRtottttttttttttttttttttt"<<rtt<<"\n";
  //timebeginlast=time::steady_clock::now();

  RtoInFlightTimeoutSegment=0;   //reset the timeout segment number

  bool hasTimeout = false;   //先置为false

  for (auto& entry : m_segmentInfo) {
    SegmentInfo& segInfo = entry.second;
    if (segInfo.state != SegmentState::InRetxQueue && // do not check segments currently in the retx queue  //在重传队列里
        segInfo.state != SegmentState::RetxReceived) { // or already-received retransmitted segments   //已经被应答的重传包
      Milliseconds timeElapsed = time::steady_clock::now() - segInfo.timeSent;   //当前时间减去发出时间
      //if (timeElapsed.count() > segInfo.rto.count()) { //失效
      if (timeElapsed.count() > 30000) {
        hasTimeout = true;   //检查超时
        enqueueForRetransmission(entry.first);   //排队重新请求
        RtoInFlightTimeoutSegment++;   //count the timeout segment number in the RTT
        TimeoutCountBbrInterval++;
      }
    }
  }

  //m_log<<"TimeoutData:"<<TimeoutCountBbrInterval<<"\n";
  //m_log<<"TimeoutRate:"<<double(TimeoutCountBbrInterval)/double(BbrCheckIntervalexpense_ms.count())*1000<<"\n";
  //DeliveredDataBbrInterval=0;


  if (hasTimeout) {   //超时
    recordTimeout();   //记录超时

    //AdjustWindow();   //最好不要在这里减，减的太频繁

    schedulePackets();   //有需要重新安排请求的分块
  }

  LastRtoInFlightsegment=m_nInFlight;

  //安排下一个检查的事件（在预定的间隔）,这样就保证之内的程序运行完
  // schedule the next check after predefined interval
  m_checkRtoEvent = m_scheduler.scheduleEvent(m_options.rtoCheckInterval, [this] { checkRto(); });   //这个间隔与rto无关，主要是定时处理需要重传的,实际间隔和处理速度有关，在设定的周围，跟测试点也有关                                                                                                  //时间很短，基本保证超时的数据包就被立即处理
}

void
PipelineInterestsAimd::sendInterest(uint64_t segNo, bool isRetransmission)   //发送兴趣包
{//m_log<<"sI"<<"\n";
  if (Filedlg1::DownLoadStatus==2)   //下载状态2
    return;

  while (Filedlg1::DownLoadStatus==1)   //下载状态1
  {
     outfile.close();
  }

  if (segNo>SentInterestHighNo)   //Update the highest interest number has been delivered
      SentInterestHighNo=segNo;

  if (m_hasFinalBlockId && segNo > m_lastSegmentNo && !isRetransmission)
    return;

  if (!isRetransmission && m_hasFailure)
    return;

  if (m_options.isVerbose) {
    std::cerr << (isRetransmission ? "Retransmitting" : "Requesting")
              << " segment #" << segNo << std::endl;
  }

  if (isRetransmission) {   //是重新请求
    // keep track of retx count for this segment
    auto ret = m_retxCount.emplace(segNo, 1);
    if (ret.second == false) { // not the first retransmission
      m_retxCount[segNo] += 1;
      if (m_retxCount[segNo] > m_options.maxRetriesOnTimeoutOrNack) {
        return handleFail(segNo, "Reached the maximum number of retries (" +
                          to_string(m_options.maxRetriesOnTimeoutOrNack) +
                          ") while retrieving segment #" + to_string(segNo));
      }

      if (m_options.isVerbose) {
        std::cerr << "# of retries for segment #" << segNo
                  << " is " << m_retxCount[segNo] << std::endl;
      }
    }

    m_face.removePendingInterest(m_segmentInfo[segNo].interestId);
  }

  Interest interest(Name(m_prefix).appendSegment(segNo));   //创建兴趣包
  //interest.setInterestLifetime(m_options.interestLifetime);
  interest.setInterestLifetime(time::milliseconds(30000));   //这个有关？？10000？

  interest.setMustBeFresh(m_options.mustBeFresh);
  interest.setMaxSuffixComponents(1);

  auto interestId = m_face.expressInterest(interest,   //发出请求
                                           bind(&PipelineInterestsAimd::handleData, this, _1, _2),
                                           bind(&PipelineInterestsAimd::handleNack, this, _1, _2),
                                           bind(&PipelineInterestsAimd::handleLifetimeExpiration,
                                                this, _1));

  //m_log<<"sendInterest:"<<segNo<<"   "<<isRetransmission<<"\n";

  // m_log<<"0000   "<<segNo<<endl;

  m_nInFlight++;   //在请求的分块数加一

  SentDataBbrInterval++;

  if (isRetransmission) {   //是重新的请求
    SegmentInfo& segInfo = m_segmentInfo[segNo];   //保留上次信息
    segInfo.timeSent = time::steady_clock::now();   //更改其他信息
    segInfo.rto = m_rttEstimator.getEstimatedRto();   //给rto赋值
    segInfo.state = SegmentState::Retransmitted;
    //segInfo.DeliveredDataSent=DeliveredData;
    //segInfo.SentDataSent=SentData;
    m_nRetransmitted++;
  }
  else {
    m_highInterest = segNo;
    m_segmentInfo[segNo] = {interestId,
                            time::steady_clock::now(),
                            m_rttEstimator.getEstimatedRto(),
                            SegmentState::FirstTimeSent,
                            DeliveredData,
                            LossData,
                            SentData};
  }

  SentData++;

  //m_log<<"sent:"<<segNo<<"   "<<"DeliveredData:"<<DeliveredData<<"\n";
}

void
PipelineInterestsAimd::schedulePackets()   //安排数据包   //相当于安排投递
{//m_log<<"schedulePackets"<<"\n";
  BOOST_ASSERT(m_nInFlight >= 0);


  auto availableWindowSize = static_cast<int64_t>(m_cwnd) - m_nInFlight;   //可用通道数=拥塞尺寸-正在请求的分块数
  //m_log<<"m_cwnd:"<<m_cwnd<<"   m_nInFlight:"<<m_nInFlight<<"\n";

  while (availableWindowSize > 0) {   //可用通道数大于0
    if (!m_retxQueue.empty()) {   //如果重传队列非空，即先处理重传队列
      uint64_t retxSegNo = m_retxQueue.front();   //取重传队列地一个元素
      m_retxQueue.pop();   //删除重传队列地一个元素

      auto it = m_segmentInfo.find(retxSegNo);   //在正在处理的分块信息里查找
      if (it == m_segmentInfo.end()) {   //如果没有了则不进行重传
        continue;
      }
      // the segment is still in the map, it means that it needs to be retransmitted
      sendInterest(retxSegNo, true);   //分块号仍在分块信息里，进行重传
    }
    else {   //重传队列空，进行下一个请求
      sendInterest(getNextSegmentNo(), false);
    }
    availableWindowSize--;    //可用通道数减一
  }
}

void
PipelineInterestsAimd::handleData(const Interest& interest, const Data& data)   //收到数据包处理
{//m_log<<"handleData:"<<getSegmentFromPacket(data)<<"\n";


  if (Filedlg1::DownLoadStatus==2)   //下载状态2
    return;

  DeliveredDataBbrInterval++;

  DeliveredData++;


  // Data name will not have extra components because MaxSuffixComponents is set to 1
  BOOST_ASSERT(data.getName().equals(interest.getName()));   //保证数据包和兴趣包的内容名字一样

  if (!m_hasFinalBlockId && !data.getFinalBlockId().empty()) {
    m_lastSegmentNo = data.getFinalBlockId().toSegment();
    m_hasFinalBlockId = true;
    cancelInFlightSegmentsGreaterThan(m_lastSegmentNo);
    if (m_hasFailure && m_lastSegmentNo >= m_failedSegNo) {
      // previously failed segment is part of the content
      return onFailure(m_failureReason);
    } else {
      m_hasFailure = false;
    }
  }

  uint64_t recvSegNo = getSegmentFromPacket(data);   //从数据包获取分块号
  SegmentInfo& segInfo = m_segmentInfo[recvSegNo];   //在处理分块信息的更新，如果是重传已接收的分块，则直接擦除后return



  if (segInfo.state == SegmentState::RetxReceived) {
    m_segmentInfo.erase(recvSegNo);

    return; // ignore already-received segment   //忽视已经收到过的分块
  }

  Milliseconds rtt = time::steady_clock::now() - segInfo.timeSent;
  if (m_options.isVerbose) {
    std::cerr << "Received segment #" << recvSegNo
              << ", rtt=" << rtt.count() << "ms"
              << ", rto=" << segInfo.rto.count() << "ms" << std::endl;
  }

  if (m_highData < recvSegNo) {   //更新最大的分块号数据
    m_highData = recvSegNo;
  }

  // for segments in retx queue, we must not decrement m_nInFlight
  // because it was already decremented when the segment timed out
  if (segInfo.state != SegmentState::InRetxQueue) {   //在排队请求的分块，不能让m_nInFlight减一，已经在超时里面处理
    //m_nInFlight--;
      if (m_nInFlight>1)
          m_nInFlight--;
  }

  m_nReceived++;   //接收到的分块加一
  m_receivedSize += data.getContent().value_size();   //接收到的尺寸

  //if (BbrState==0||BbrState==2)
  //if (BbrState==0)
  //{
//      increaseWindow();   //增加窗口尺寸
  //}
  onData(interest, data);   //数据处理


  if (m_cwnd<=10)
  {
      increaseWindow();   //增加窗口尺寸
  }
  else
  {
  //DQN数据记录
  uint64_t SegmentDeliveredData=DeliveredData-segInfo.DeliveredDataSent;
  uint64_t SegmentLossData=LossData-segInfo.LossDataSent;
  Milliseconds SegmentRTT = time::steady_clock::now() - segInfo.timeSent;
  uint64_t SegmentSentData=SentData-segInfo.SentDataSent;
  SegmentDeliveryRate=(double(std::min(SegmentDeliveredData,SegmentSentData))/double(SegmentRTT.count()));
  m_log<<m_cwnd<<"   "<<m_nInFlight<<"   "<<SegmentDeliveredData<<"   "<<SegmentLossData<<"   "<<SegmentRTT.count()<<"   "<<SegmentDeliveryRate<<"\n";

  //DQN窗口控制
  Py_Initialize();
  if ( !Py_IsInitialized() )
  {
      return;
  }
  PyRun_SimpleString("import sys");
  PyRun_SimpleString("sys.path.append('./')");
  PyRun_SimpleString("sys.path.append('../../Python/')");

  PyObject *pModule;
  pModule = PyImport_ImportModule("UseGraph");
  if ( !pModule )
  {
      cout<<"can't find UseGraph.py"<<endl;
      getchar();
      return;
  }
  PyObject *pDict;
  pDict = PyModule_GetDict(pModule);
  if ( !pDict )
  {
      cout<<"can't find dictionarys";
      return;
  }
  PyObject *pFunc;
  pFunc = PyDict_GetItemString(pDict, "LoadGraph");
  if ( !pFunc || !PyCallable_Check(pFunc) )
  {
      cout<<"can't find function [LoadGraph]"<<endl;
      getchar();
      return;
  }

  PyObject *result = NULL;
  PyObject *pArgs = PyTuple_New(4);
  PyTuple_SetItem(pArgs, 0, Py_BuildValue("f", m_nInFlight));
  PyTuple_SetItem(pArgs, 1, Py_BuildValue("f", SegmentDeliveredData));
  PyTuple_SetItem(pArgs, 2, Py_BuildValue("f", SegmentLossData));
  PyTuple_SetItem(pArgs, 3, Py_BuildValue("f", SegmentRTT));
  result = PyObject_CallObject(pFunc, pArgs);
  int resultt=0;
  PyArg_Parse(result,"i",&resultt);
  cout<<"m_cwnd1:"<<m_cwnd<<"   result:"<<resultt<<endl;
  if (resultt==1)
      m_cwnd=m_cwnd+1;
  else if (resultt==2)
      m_cwnd=m_cwnd-1;
  else if (resultt==3)
      m_cwnd=m_cwnd*2;
  else if (resultt==4)
      m_cwnd=m_cwnd/2;
  else if (resultt==5)
      m_cwnd=m_cwnd*5/4;
  else if (resultt==6)
      m_cwnd=m_cwnd*4/5;
  cout<<"DQN"<<endl;
  }
  cout<<"m_cwnd:"<<m_cwnd<<endl;

/*
  if (segInfo.state == SegmentState::FirstTimeSent)
  {
  //投递数据计算
  //m_log<<"recvSegNo:"<<recvSegNo<<"\n";
  uint64_t SegmentDeliveredData=DeliveredData-segInfo.DeliveredDataSent;
  uint64_t SegmentSentData=SentData-segInfo.SentDataSent;

  Milliseconds SegmentRTT = time::steady_clock::now() - segInfo.timeSent;
  //if (SegmentRTT.count()<=2000000)
  //{
  SegmentDeliveryRate=(double(std::min(SegmentDeliveredData,SegmentSentData))/double(SegmentRTT.count()))*1000;
  //SegmentDeliveryRatio=double(SegmentDeliveredData)/double(SegmentSentData);

  //m_log<<"1111   "<<recvSegNo<<"   "<<segInfo.DeliveredDataSent<<"   "<<DeliveredData<<"   "<<SegmentDeliveredData<<"   "<<SegmentSentData<<"   "<<double (SegmentRTT.count())<<"   "<<SegmentDeliveryRate<<"   "<<SegmentDeliveryRatio<<"   "<<m_cwnd<<"   "<<m_nInFlight<<"\n";
  //m_log<<recvSegNo<<"   "<<double (SegmentRTT.count())<<"   "<<SegmentDeliveryRate<<"   "<<m_cwnd<<"\n";


  }

  //}
*/


//m_log<<"Rto:"<<m_rttEstimator.getEstimatedRto()<<"\n";




  if (segInfo.state == SegmentState::FirstTimeSent ||
      segInfo.state == SegmentState::InRetxQueue) { // do not sample RTT for retransmitted segments   //不对重新请求的分块取样（它可能被上个请求到到了途中，不准确）
    auto nExpectedSamples = std::max<int64_t>((m_nInFlight + 1) >> 1, 1);
    BOOST_ASSERT(nExpectedSamples > 0);
    m_rttEstimator.addMeasurement(recvSegNo, rtt, static_cast<size_t>(nExpectedSamples));   //RTT采样，除了地一个用initial，其它的用这个
    m_segmentInfo.erase(recvSegNo); // remove the entry associated with the received segment
    RttMeasurement(rtt);
    RttUpdateFlag=true;
    //m_log<<"handledata:"<<recvSegNo<<"   RttMeasurement:"<<rtt<<"\n";
  }
  else { // retransmission   //重新请求
    BOOST_ASSERT(segInfo.state == SegmentState::Retransmitted);
    segInfo.state = SegmentState::RetxReceived;
  }

  BOOST_ASSERT(m_nReceived > 0);
  if (m_hasFinalBlockId &&
      static_cast<uint64_t>(m_nReceived - 1) >= m_lastSegmentNo) { // all segments have been received   //所有的分块都已经接收到
    cancel();   //取消
    if (m_options.isVerbose) {
      printSummary();
    }
  }
  else {
    schedulePackets();   //收到数据了，可以安排下一个请求
  }
}

void
PipelineInterestsAimd::RttMeasurement(Milliseconds rtt)   //这样取Rtt更准确   //取三个队列的均值来平滑？
{
  //rtt=(rtt<minRtt)?minRtt:(rtt>maxRtt)?maxRtt:rtt;
  RttValue=boost::chrono::duration_cast<boost::chrono::milliseconds>(rtt);
  //if (RttValueQueueCount<=2)
  //{
  //    RttValueQueue[RttValueQueueCount]=RttValue;
  //}

  //BbrCheckInterval=RttValue;   //?
  //BbrCheckInterval=boost::chrono::duration_cast<boost::chrono::milliseconds>(m_rttEstimator.getEstimatedRto());
//  BbrCheckInterval=RttValue;

  //m_log<<"RttValue:"<<RttValue<<"\n";
}

void
PipelineInterestsAimd::handleNack(const Interest& interest, const lp::Nack& nack)   //否定应答处理
{//m_log<<"handleNack:"<<getSegmentFromPacket(interest)<<"\n";
  if (Filedlg1::DownLoadStatus==2)   //下载停止状态
    return;

  DeliveredDataBbrInterval++;

  DeliveredData++;

  if (m_options.isVerbose)
    std::cerr << "Received Nack with reason " << nack.getReason()
              << " for Interest " << interest << std::endl;

  uint64_t segNo = getSegmentFromPacket(interest);

  switch (nack.getReason()) {   //收到否定应答原因
    case lp::NackReason::DUPLICATE:
      // ignore duplicates
      break;
    case lp::NackReason::CONGESTION:   //拥塞
      // treated the same as timeout for now   //和超时处理一样
      enqueueForRetransmission(segNo);
      recordTimeout();

      //m_log<<getSegmentFromPacket(interest)<<":NackReason:CONGESTION";

      schedulePackets();   //收打NACK了，可以安排下一个请求
      break;
    default:
      handleFail(segNo, "Could not retrieve data for " + interest.getName().toUri() +
                 ", reason: " + boost::lexical_cast<std::string>(nack.getReason()));
      break;
  }
}

void
PipelineInterestsAimd::handleLifetimeExpiration(const Interest& interest)   //兴趣包有效期到期处理
{//m_log<<"handleLifetimeExpiration:"<<getSegmentFromPacket(interest)<<"\n";
  if (Filedlg1::DownLoadStatus==2)   //下载停止状态
    return;

  enqueueForRetransmission(getSegmentFromPacket(interest));   //排队重新请求
  recordTimeout();   //记录超时

  schedulePackets();   //兴趣的生命期到了可以安排下一个请求
}

void
PipelineInterestsAimd::recordTimeout()   //记录超时
{//m_log<<"recordTimeout:";
    //m_log<<m_highData<<"   "<<m_recPoint<<"   *";

  if (m_options.disableCwa || m_highData > m_recPoint) {   //每个RTT仅响应一次超时（保守窗口适配）   //收到数据包的最大编号比上一个RTT的兴趣包编号大
                                                           //上一个RTT内的发出的兴趣包要接收完成，否则窗口维持不变   bbr的4/5填补？
    // react to only one timeout per RTT (conservative window adaptation)
    m_recPoint = m_highInterest;

    //m_log<<m_highInterest<<"\n";

//    decreaseWindow();   //积式减少窗口尺寸
    m_rttEstimator.backoffRto();
    m_nLossEvents++;   //丢包数加一
LossData++;
    if (m_options.isVerbose) {
      std::cerr << "Packet loss event, cwnd = " << m_cwnd
                << ", ssthresh = " << m_ssthresh << std::endl;
    }
  }
}

void
PipelineInterestsAimd::enqueueForRetransmission(uint64_t segNo)   //排队重新请求
{//m_log<<"enqueueForRetransmission:"<<segNo<<"\n";
  //BOOST_ASSERT(m_nInFlight > 0);   //保证正在请求数大于0
BOOST_ASSERT(m_nInFlight >=0);
  //m_nInFlight--;
//  if (m_nInFlight>1)
    if (m_nInFlight>0)
      m_nInFlight--;

  m_retxQueue.push(segNo);
  m_segmentInfo.at(segNo).state = SegmentState::InRetxQueue;
}

void
PipelineInterestsAimd::handleFail(uint64_t segNo, const std::string& reason)   //失败处理
{//m_log<<"handleFail:"<<segNo<<"\n";
  if (Filedlg1::DownLoadStatus==2)   //停止下载
    return;

  // if the failed segment is definitely part of the content, raise a fatal error
  if (m_hasFinalBlockId && segNo <= m_lastSegmentNo)   //如果分块是所需要请求的分块，上升错误等级
    return onFailure(reason);

  if (!m_hasFinalBlockId) {   //没有最末端的分块号
    m_segmentInfo.erase(segNo);
    //m_nInFlight--;   //正在请求数减一
    if (m_nInFlight>1)
        m_nInFlight--;

    if (m_segmentInfo.empty()) {
      onFailure("Fetching terminated but no final segment number has been found");
    }
    else {
      cancelInFlightSegmentsGreaterThan(segNo);
      m_hasFailure = true;
      m_failedSegNo = segNo;
      m_failureReason = reason;
    }
  }
}

void
PipelineInterestsAimd::increaseWindow()   //和式增加
{//m_log<<"increaseWindow"<<"\n";

  //m_cwnd +=1;


    if (BbrState==0)
    {
      m_cwnd += m_options.aiStep; // additive increase   //窗口尺寸+=和式增加值
    }
    else if (BbrState==5)
    {   //基本保持了，加的很小
      m_cwnd += 4.0 / std::floor(m_cwnd); // congestion avoidance   //避免拥塞，窗口尺寸+=和式增加值/当前窗口尺寸向下取整
    }



  /*
  if (m_cwnd < m_ssthresh) {
    m_cwnd += m_options.aiStep; // additive increase   //窗口尺寸+=和式增加值
  } else {   //基本保持了，加的很小
    m_cwnd += m_options.aiStep / std::floor(m_cwnd); // congestion avoidance   //避免拥塞，窗口尺寸+=和式增加值/当前窗口尺寸向下取整
  }

  //if (m_cwnd>30)
  //    m_cwnd=30;   //?
  */

  afterCwndChange(time::steady_clock::now() - getStartTime(), m_cwnd);   //发出更改Window的信号
}

void
PipelineInterestsAimd::AdjustWindow()
{//m_log<<"AdjustWindow"<<"\n";

  //m_log<<"RtoInFlightTimeoutSegment:"<<RtoInFlightTimeoutSegment<<"   m_nInFlight:"<<m_nInFlight<<"   aaa:"<<RtoInFlightTimeoutSegment/double(m_nInFlight)<<"\n";

  if (m_hasFinalBlockId && SentInterestHighNo < m_lastSegmentNo&&LastRtoInFlightsegment!=0)   //兴趣包分发还没有完成，保证后面只有重传时的速率
  {
     //加权
     //if (RtoInFlightTimeoutSegment/LastRtoInFlightsegment>loss_threshold)
     m_cwnd=m_cwnd*(1-RtoInFlightTimeoutSegment/LastRtoInFlightsegment);   //RtoInFlightTimeoutSegment/LastRtoInFlightsegment表示当前RTT内的丢包率

     afterCwndChange(time::steady_clock::now() - getStartTime(), m_cwnd);   //发出更改Window的信号
  }
}

void
PipelineInterestsAimd::decreaseWindow()   //积式减少
{//m_log<<"decreaseWindow"<<"\n";
  // please refer to RFC 5681, Section 3.1 for the rationale behind it

  m_ssthresh = std::max(2.0, m_cwnd * m_options.mdCoef); // multiplicative decrease   //缓慢启动阈值（>=2）=当前窗口尺寸×积式减少系数
    m_cwnd = m_options.resetCwndToInit ? m_options.initCwnd : m_ssthresh;   //窗口尺寸=初始化尺寸/缓慢启动阈值   m=a<b?a:b

//  if (m_cwnd>30)
//      m_cwnd=30;   //?

  afterCwndChange(time::steady_clock::now() - getStartTime(), m_cwnd);   //发出更改Window的信号
}

uint64_t
PipelineInterestsAimd::getNextSegmentNo()
{
  Filedlg1::TotalTime=time.elapsed();   //算出消耗时间
  // get around the excluded segment
  if (m_nextSegmentNo == m_excludedSegmentNo)   //绕过需要排除的分块号
    m_nextSegmentNo++;
  return m_nextSegmentNo++;   //加一得出下一个需要提取的分块号
}

void
PipelineInterestsAimd::cancelInFlightSegmentsGreaterThan(uint64_t segNo)
{
  for (auto it = m_segmentInfo.begin(); it != m_segmentInfo.end();) {
    // cancel fetching all segments that follow
    if (it->first > segNo) {   //取消对当前分块之后的分块的提取
      m_face.removePendingInterest(it->second.interestId);   //移除待处理兴趣
      it = m_segmentInfo.erase(it);
      //m_nInFlight--;   //正在请求数减一
      if (m_nInFlight>1)
         m_nInFlight--;

    }
    else {
      ++it;
    }
  }
}

void
PipelineInterestsAimd::printSummary() const
{
  Milliseconds timeElapsed = time::steady_clock::now() - getStartTime();
  double throughput = (8 * m_receivedSize * 1000) / timeElapsed.count();

  int pow = 0;
  std::string throughputUnit;
  while (throughput >= 1000.0 && pow < 4) {
    throughput /= 1000.0;
    pow++;
  }
  switch (pow) {
    case 0:
      throughputUnit = "bit/s";
      break;
    case 1:
      throughputUnit = "kbit/s";
      break;
    case 2:
      throughputUnit = "Mbit/s";
      break;
    case 3:
      throughputUnit = "Gbit/s";
      break;
    case 4:
      throughputUnit = "Tbit/s";
      break;
  }

  std::cerr << "\nAll segments have been received.\n"
            << "Time elapsed: " << timeElapsed << "\n"
            << "Total # of segments received: " << m_nReceived << "\n"
            << "Total # of packet loss events: " << m_nLossEvents << "\n"
            << "Packet loss rate: "
            << static_cast<double>(m_nLossEvents) / static_cast<double>(m_nReceived) << "\n"
            << "Total # of retransmitted segments: " << m_nRetransmitted << "\n"
            << "Goodput: " << throughput << " " << throughputUnit << "\n";
}

std::ostream&
operator<<(std::ostream& os, SegmentState state)
{
  switch (state) {
  case SegmentState::FirstTimeSent:
    os << "FirstTimeSent";
    break;
  case SegmentState::InRetxQueue:
    os << "InRetxQueue";
    break;
  case SegmentState::Retransmitted:
    os << "Retransmitted";
    break;
  case SegmentState::RetxReceived:
    os << "RetxReceived";
    break;
  }
  return os;
}

std::ostream&
operator<<(std::ostream& os, const PipelineInterestsAimdOptions& options)
{
  os << "PipelineInterestsAimd initial parameters:\n"
     << "\tInitial congestion window size = " << options.initCwnd << "\n"
     << "\tInitial slow start threshold = " << options.initSsthresh << "\n"
     << "\tAdditive increase step = " << options.aiStep << "\n"
     << "\tMultiplicative decrease factor = " << options.mdCoef << "\n"
     << "\tRTO check interval = " << options.rtoCheckInterval << "\n"
     << "\tMax retries on timeout or Nack = " << options.maxRetriesOnTimeoutOrNack << "\n"
     << "\tConservative Window Adaptation " << (options.disableCwa ? "disabled" : "enabled") << "\n"
     << "\tResetting cwnd to " << (options.resetCwndToInit ? "initCwnd" : "ssthresh") << " upon loss event\n";
  return os;
}

} // namespace aimd
} // namespace chunks
} // namespace ndn
