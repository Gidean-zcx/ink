using namespace Pinetime::Applications::Screens;

void Timer::Refresh() {
  auto timerStatus = timer.GetTimerState();

  if (timerStatus && timerStatus->expired) {
    // Timer exists and has expired, so we're in ringing mode
    DisplayTime();

    if (timerStatus->distanceToExpiry.count() > 60000 && motorController.IsRinging()) {
      // Stop buzzing after 10 seconds, but continue the counter
      motorController.StopRinging();
      wakeLock.Release();
    }

  } else if (timer.IsRunning()) {
    DisplayTime();
  }
}

void Timer::DisplayTime() {
  displaySeconds =
    std::chrono::ceil<std::chrono::seconds>(timer.GetTimerState().value_or(Controllers::Timer::TimerStatus {}).distanceToExpiry);
  if (displaySeconds.IsUpdated()) {
    hourCounter.SetValue(displaySeconds.Get().count() / 3600);
    minuteCounter.SetValue(displaySeconds.Get().count() / 60 % 60);
    secondCounter.SetValue(displaySeconds.Get().count() % 60);
  }
}

void Timer::ToggleRunning() {
  auto timerStatus = timer.GetTimerState();
  if (timerStatus && timerStatus->expired) {
    motorController.StopRinging();
    Reset();
  } else if (timer.IsRunning()) {
    DisplayTime();
    timer.StopTimer();
    SetTimerStopped();
  } else if (secondCounter.GetValue() + minuteCounter.GetValue() + hourCounter.GetValue() > 0) {
    auto timerDuration = std::chrono::hours(hourCounter.GetValue()) + std::chrono::minutes(minuteCounter.GetValue()) + std::chrono::seconds(secondCounter.GetValue());
    timer.StartTimer(timerDuration);
    Refresh();
    SetTimerRunning();
  }
}

void Timer::Reset() {
  timer.ResetExpiredTime();
  DisplayTime();
  SetTimerStopped();
}
