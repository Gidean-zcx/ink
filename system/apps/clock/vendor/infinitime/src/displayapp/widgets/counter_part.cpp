using namespace Pinetime::Applications::Widgets;

void Counter::UpBtnPressed() {
  value++;
  if (value > max) {
    value = min;
  }
  UpdateLabel();

  if (ValueChangedHandler != nullptr) {
    ValueChangedHandler(userData);
  }
};

void Counter::DownBtnPressed() {
  value--;
  if (value < min) {
    value = max;
  }
  UpdateLabel();

  if (ValueChangedHandler != nullptr) {
    ValueChangedHandler(userData);
  }
};

void Counter::SetValue(int newValue) {
  value = newValue;
  UpdateLabel();
}
