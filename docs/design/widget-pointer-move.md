# WID-08 Mini入口与指针拖动

FR-012/018，UX-09～14；沿HIG清楚操作/直接操作/可恢复，复用现有三形态和统一画布，不新增形态。Mini小太阳与摘要均调用activatePrimary：Panel可用则临时展开，Panel禁用则打开主窗；不能仅selectedForm为Ball时生效。按钮有可访问动作名、键盘可达；拖动取消按钮点击。

原DragHandler在取得Qt指针抓取后调用startSystemMove，跨原生嵌套事件循环可能取消handler并错序设置移动状态。改为共享WidgetMoveHandle（DragHandler）：激活→beginPointerMove(globalPress)，平移→movePointer(globalPosition)，释放/取消→finishMove。C++记录固定的窗口/指针原点，用平台move端口设置位置，不更改窗口flags、不交接系统抓取。Panel/Mini/Ball同一实现；置顶和锁定是独立状态，锁定中途结束移动，菜单/穿透/过渡期间不启动。

接口返回bool/现有WindowError，失败不残留moving；自动收球计时在移动期间暂停，结束后捕获逻辑位置/锚点并恢复。指针模式以handler释放为准，不用全局mouseButtons轮询误判合成或丢失事件。已有startMove API保留供平台调用方，QML拖动不再使用。

无协议/数据库变化，位置仍用既有本地偏好。模块单测验证位移/取消/锁定/置顶与错误恢复；真实QML验证太阳点击、拖动不误点、连续移动、负屏/缩放；原生Windows观察实际坐标，物理多屏/触控与其他平台另列。自审通过，现行结果见PROGRESS。
