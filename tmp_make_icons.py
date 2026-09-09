# -*- coding: utf-8 -*-
import os
from PIL import Image

src = r"C:\Users\wxw\Desktop\witt 应用图标.jpg"
out_dir = r"c:\Users\wxw\Desktop\QT\TASoftware\forensen_combnitation\res"
os.makedirs(out_dir, exist_ok=True)

im = Image.open(src).convert("RGBA")

# ico 多尺寸（16~256）
sizes = [(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48),
         (64, 64), (128, 128), (256, 256)]
im.save(os.path.join(out_dir, "app.ico"), format="ICO", sizes=sizes)

# png 供 Qt 运行时窗口/任务栏/菜单图标
im.resize((256, 256), Image.LANCZOS).save(os.path.join(out_dir, "app.png"), format="PNG")

print("done:", os.listdir(out_dir))
