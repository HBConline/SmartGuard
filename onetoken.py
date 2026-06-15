#!/usr/bin/env python3
"""OneNET MQTT Token 生成器 - HMAC-SHA1 签名，纯标准库"""
import time, hmac, hashlib, base64, urllib.parse

import os

PRODUCT_ID = os.environ.get("ONENET_PRODUCT_ID", "YOUR_PRODUCT_ID")
DEVICE_NAME = os.environ.get("ONENET_DEVICE_NAME", "YOUR_DEVICE_NAME")
DEVICE_KEY = os.environ.get("ONENET_DEVICE_KEY", "YOUR_DEVICE_KEY")
VERSION = "2018-10-31"
METHOD = "sha1"
TOKEN_LIFETIME = 3600  # 1小时


def generate_token():
    """生成 OneNET MQTT password token"""
    res = f"products/{PRODUCT_ID}/devices/{DEVICE_NAME}"
    et = int(time.time()) + TOKEN_LIFETIME

    # HMAC-SHA1 签名
    key_bytes = base64.b64decode(DEVICE_KEY)
    sign_string = f"{et}\n{METHOD}\n{res}\n{VERSION}"
    signature = hmac.new(key_bytes, sign_string.encode(), hashlib.sha1).digest()
    sign_b64 = base64.b64encode(signature).decode()

    # URL 编码
    res_enc = urllib.parse.quote(res, safe='')
    sign_enc = urllib.parse.quote(sign_b64, safe='')

    token = f"version={VERSION}&res={res_enc}&et={et}&method={METHOD}&sign={sign_enc}"
    return token


def get_expiry():
    """获取当前 token 的过期时间"""
    return int(time.time()) + TOKEN_LIFETIME - 60  # 提前 1 分钟刷新


if __name__ == "__main__":
    t = generate_token()
    print(f"Token: {t[:80]}...")
    print(f"Expires: {time.ctime(get_expiry())}")
