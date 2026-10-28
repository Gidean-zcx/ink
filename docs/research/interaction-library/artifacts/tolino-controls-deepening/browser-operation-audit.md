# 公开选馆器有限实际操作审计

E-TLCD-07；F-M058-19。2026-10-27（UTC日志为10月26日晚），Linux Chromium156.0.8078.4，1280×950，de-DE，新建匿名上下文；研究输入Hamburg和zzqvexample，不是用户所在城市。目标是公开选馆器，而非Tolino设备、已登录借书或Onleihe全站。

从公开Tolino首页进入，接受仅必要cookies，打开选馆器，填无匹配词再改Hamburg，点击Bücherhallen Hamburg；外部Onleihe欢迎页仍要求选馆。随后浏览器后退，点可见左上箭头回Tolino首页并刷新，出现最近选馆记录。这里未登录，不能把该记录当成已取得借阅授权。会话以quit正常关闭，exit0；没有账号、凭证、借书、购买、消息或文件下载。

10次状态捕获；逐图查看01/03/04/05/07/08/10，正式保留6张（03为空白过渡帧）。未查看的02/06/09不冒充视觉确认。截图和随后DOM按顺序采集，不是原子状态：01图尚无cookie层而DOM已有；03图为空白而DOM有列表；09早期DOM尚无最近项，10刷新后明确出现。不能把这些过渡差异当成稳定产品状态。

已观察：无结果空列表、两条Hamburg结果、外部欢迎页的重新选馆分支、后退后输入词和列表不一致、匿名同会话刷新后的最近项。未观察：再登录、借阅成功、退出登录、清浏览器数据后的结果、跨设备同步、真实T302点击和刷新。帮助承诺直接登录与本次落点不符，原因可能包括服务迁移或上下文条件，未定位根因，不能宣布所有用户均失败。

[逐状态记录](browser/observed-states.json)、[实际命令日志](browser/trace-actions.json)、[错误记录](browser/browser-errors.json)、[原始捕获散列](browser/raw-capture-manifest.json)。命令日志是Playwright显式动作日志，不是浏览器原生trace包；没有用脚本修改产品内部状态。

- [进入页](browser/01-home.png)
- [无结果](browser/04-no-matches.png)
- [匹配结果](browser/05-hamburg-results.png)
- [外部欢迎页](browser/07-library-loaded.png)
- [后退后的输入与列表](browser/08-back-to-finder.png)
- [刷新后最近选择](browser/10-home-reloaded.png)
