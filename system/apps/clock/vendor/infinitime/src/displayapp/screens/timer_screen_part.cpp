using namespace Pinetime::Applications::Screens;

void Timer::Refresh() {
  auto timerStatus = timer.GetTimerState();

  if (timerStatus && timerStatus->expired) {
    // Timer exists and has expired, so we're in ringing mode
    DisplayTime();

    if (timerStatus->distanceToExpiry.count() > 10000 && motorController.IsRinging()) {
      // Stop buzzing after 10 seconds, but continue the counter
      motorController.StopRinging();
      wakeLock.Release();
    }

    // Reset timer after 1 minute
    if (timerStatus->distanceToExpiry.count() > 60000) {
      Reset();
    }
  } else if (timer.IsRunning()) {
    DisplayTime();
  } else if (buttonPressing && xTaskGetTickCount() - pressTime > pdMS_TO_TICKS(150)) {
    lv_label_set_text_static(txtPlayPause, "Reset");
    maskPosition += 15;
    if (maskPosition > 240) {
      MaskReset();
      Reset();
    } else {
      UpdateMask();
    }
  }
}

void Timer::DisplayTime() {
  displaySeconds =
    std::chrono::duration_cast<std::chrono::seconds>(timer.GetTimerState().value_or(Controllers::Timer::TimerStatus {}).distanceToExpiry);
  if (displaySeconds.IsUpdated()) {
    minuteCounter.SetValue(displaySeconds.Get().count() / 60);
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
  } else if (secondCounter.GetValue() + minuteCounter.GetValue() > 0) {
    auto timerDuration = std::chrono::minutes(minuteCounter.GetValue()) + std::chrono::seconds(secondCounter.GetValue());
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
