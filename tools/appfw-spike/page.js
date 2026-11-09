title("阅读偏好");
toggle("显示进度", true);
toggle("显示页码", false);
toggle("记住位置", true);
toggle("阅读提醒", false);
toggle("词语提示", true);
toggle("显示注释", false);
toggle("听书续播", true);
toggle("显示书名", false);
textfield("备注", "每天读一会儿");
button("完成");
function on_toggle(row, value) {
  set(row, value);
}
