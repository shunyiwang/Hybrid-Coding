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
  , m_checkRtoEvent(m_scheduler)
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
{
  if (m_options.isVerbose) {
    std::cerr << m_options;
  }

  time.start();
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

  schedulePackets();   //安排请求
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

void   //简单的拥塞判断
PipelineInterestsAimd::checkRto()   //检查Rto中所有发出但还未应答的请求（每次检查完，设定一定时间间隔的下一次检查）
{
  if (Filedlg1::DownLoadStatus==2)   //下载停止状态
    return;

  bool hasTimeout = false;   //先置为false

  for (auto& entry : m_segmentInfo) {
    SegmentInfo& segInfo = entry.second;
    if (segInfo.state != SegmentState::InRetxQueue && // do not check segments currently in the retx queue
        segInfo.state != SegmentState::RetxReceived) { // or already-received retransmitted segments
      Milliseconds timeElapsed = time::steady_clock::now() - segInfo.timeSent;   //当前时间减去发出时间
      if (timeElapsed.count() > segInfo.rto.count()) { //失效
        hasTimeout = true;   //检查超时
        enqueueForRetransmission(entry.first);   //排队重新请求
      }
    }
  }

  if (hasTimeout) {   //超时
    recordTimeout();   //记录超时
    schedulePackets();   //重新安排请求
  }

  //安排下一个检查的事件（在预定的间隔）
  // schedule the next check after predefined interval
  m_checkRtoEvent = m_scheduler.scheduleEvent(m_options.rtoCheckInterval, [this] { checkRto(); });
}

void
PipelineInterestsAimd::sendInterest(uint64_t segNo, bool isRetransmission)   //发送兴趣包
{
  if (Filedlg1::DownLoadStatus==2)   //下载状态2
    return;

  while (Filedlg1::DownLoadStatus==1)   //下载状态1
  {
  }

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
  interest.setInterestLifetime(m_options.interestLifetime);
  interest.setMustBeFresh(m_options.mustBeFresh);
  interest.setMaxSuffixComponents(1);

  auto interestId = m_face.expressInterest(interest,   //发出请求
                                           bind(&PipelineInterestsAimd::handleData, this, _1, _2),
                                           bind(&PipelineInterestsAimd::handleNack, this, _1, _2),
                                           bind(&PipelineInterestsAimd::handleLifetimeExpiration,
                                                this, _1));

  m_nInFlight++;   //在请求的分块数加一

  if (isRetransmission) {   //是重新的请求
    SegmentInfo& segInfo = m_segmentInfo[segNo];
    segInfo.timeSent = time::steady_clock::now();
    segInfo.rto = m_rttEstimator.getEstimatedRto();
    segInfo.state = SegmentState::Retransmitted;
    m_nRetransmitted++;
  }
  else {
    m_highInterest = segNo;
    m_segmentInfo[segNo] = {interestId,
                            time::steady_clock::now(),
                            m_rttEstimator.getEstimatedRto(),
                            SegmentState::FirstTimeSent};
  }
}

void
PipelineInterestsAimd::schedulePackets()   //安排数据包
{
  BOOST_ASSERT(m_nInFlight >= 0);


  /*   er bu shi zhi jie zhong duan cheng xu
  if (m_nInFlight > 0)
      m_nInFlight--;

  */





  auto availableWindowSize = static_cast<int64_t>(m_cwnd) - m_nInFlight;   //可用通道数=拥塞尺寸-正在请求的分块数

  while (availableWindowSize > 0) {   //可用通道数大于0
    if (!m_retxQueue.empty()) {   //如果转发队列为空，进行转发
      uint64_t retxSegNo = m_retxQueue.front();   //取转发队列的数据
      m_retxQueue.pop();   //移除转发队列第一个内容

      auto it = m_segmentInfo.find(retxSegNo);   //
      if (it == m_segmentInfo.end()) {   //
        continue;
      }
      // the segment is still in the map, it means that it needs to be retransmitted
      sendInterest(retxSegNo, true);   //分块号仍在表格中，需要重新转发
    }
    else {   //进行下一个请求
      sendInterest(getNextSegmentNo(), false);
    }
    availableWindowSize--;    //可用通道数减一
  }
}

void
PipelineInterestsAimd::handleData(const Interest& interest, const Data& data)   //收到数据包处理
{
  if (Filedlg1::DownLoadStatus==2)   //下载状态2
    return;

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
  SegmentInfo& segInfo = m_segmentInfo[recvSegNo];
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
      if (m_nInFlight>0)
          m_nInFlight--;
  }

  m_nReceived++;   //接收到的分块加一
  m_receivedSize += data.getContent().value_size();   //接收到的尺寸

  increaseWindow();   //增加窗口尺寸
  onData(interest, data);   //数据处理

  if (segInfo.state == SegmentState::FirstTimeSent ||
      segInfo.state == SegmentState::InRetxQueue) { // do not sample RTT for retransmitted segments   //不对重新请求的分块取样
    auto nExpectedSamples = std::max<int64_t>((m_nInFlight + 1) >> 1, 1);
    BOOST_ASSERT(nExpectedSamples > 0);
    m_rttEstimator.addMeasurement(recvSegNo, rtt, static_cast<size_t>(nExpectedSamples));
    m_segmentInfo.erase(recvSegNo); // remove the entry associated with the received segment
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
    schedulePackets();
  }
}

void
PipelineInterestsAimd::handleNack(const Interest& interest, const lp::Nack& nack)   //否定应答处理
{
  if (Filedlg1::DownLoadStatus==2)   //下载停止状态
    return;

  if (m_options.isVerbose)
    std::cerr << "Received Nack with reason " << nack.getReason()
              << " for Interest " << interest << std::endl;

  uint64_t segNo = getSegmentFromPacket(interest);

  switch (nack.getReason()) {   //收到否定应答原因
    case lp::NackReason::DUPLICATE:
      // ignore duplicates
      break;
    case lp::NackReason::CONGESTION:
      // treated the same as timeout for now
      enqueueForRetransmission(segNo);
      recordTimeout();
      schedulePackets();
      break;
    default:
      handleFail(segNo, "Could not retrieve data for " + interest.getName().toUri() +
                 ", reason: " + boost::lexical_cast<std::string>(nack.getReason()));
      break;
  }
}

void
PipelineInterestsAimd::handleLifetimeExpiration(const Interest& interest)   //兴趣包有效期到期处理
{
  if (Filedlg1::DownLoadStatus==2)   //下载停止状态
    return;

  enqueueForRetransmission(getSegmentFromPacket(interest));   //排队重新请求
  recordTimeout();   //记录超时
  schedulePackets();   //计划数据包
}

void
PipelineInterestsAimd::recordTimeout()   //记录超时
{
  if (m_options.disableCwa || m_highData > m_recPoint) {   //每个RTT仅响应一次超时（保守窗口适配）
    // react to only one timeout per RTT (conservative window adaptation)
    m_recPoint = m_highInterest;

    decreaseWindow();   //积式减少窗口尺寸
    m_rttEstimator.backoffRto();
    m_nLossEvents++;   //丢包数加一

    if (m_options.isVerbose) {
      std::cerr << "Packet loss event, cwnd = " << m_cwnd
                << ", ssthresh = " << m_ssthresh << std::endl;
    }
  }
}

void
PipelineInterestsAimd::enqueueForRetransmission(uint64_t segNo)   //排队重新请求
{
  BOOST_ASSERT(m_nInFlight > 0);   //保证正在请求数大于0

  //m_nInFlight--;
  if (m_nInFlight>0)
      m_nInFlight--;

  m_retxQueue.push(segNo);
  m_segmentInfo.at(segNo).state = SegmentState::InRetxQueue;
}

void
PipelineInterestsAimd::handleFail(uint64_t segNo, const std::string& reason)   //失败处理
{
  if (Filedlg1::DownLoadStatus==2)   //停止下载
    return;

  // if the failed segment is definitely part of the content, raise a fatal error
  if (m_hasFinalBlockId && segNo <= m_lastSegmentNo)   //如果分块是所需要请求的分块，上升错误等级
    return onFailure(reason);

  if (!m_hasFinalBlockId) {   //没有最末端的分块号
    m_segmentInfo.erase(segNo);
    //m_nInFlight--;   //正在请求数减一
    if (m_nInFlight>0)
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
{
  if (m_cwnd < m_ssthresh) {
    m_cwnd += m_options.aiStep; // additive increase   //窗口尺寸+=和式增加值
  } else {
    m_cwnd += m_options.aiStep / std::floor(m_cwnd); // congestion avoidance   //避免拥塞，窗口尺寸+=和式增加值/当前窗口尺寸向下取整
  }

  if (m_cwnd>30)
      m_cwnd=30;

  afterCwndChange(time::steady_clock::now() - getStartTime(), m_cwnd);   //发出更改Window的信号
}

void
PipelineInterestsAimd::decreaseWindow()   //积式减少
{
  // please refer to RFC 5681, Section 3.1 for the rationale behind it
  m_ssthresh = std::max(2.0, m_cwnd * m_options.mdCoef); // multiplicative decrease   //缓慢启动阈值（>=2）=当前窗口尺寸×积式减少系数
  m_cwnd = m_options.resetCwndToInit ? m_options.initCwnd : m_ssthresh;   //窗口尺寸=初始化尺寸/缓慢启动阈值

  if (m_cwnd>30)
      m_cwnd=30;

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
      if (m_nInFlight>0)
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
