#!/usr/bin/env python3
"""Generate the deterministic sample inputs used by the iss kernels.

Outputs (committed, so results do not depend on the host's zlib):
  text_zh.txt   synthetic Chinese prose with punctuation, ASCII words, digits
  text_zh.zlib  zlib stream (level 6) of text_zh.txt, input for the inflate kernel
  sample.json   an API-style JSON response (news feed), input for cJSON

The text is meaningless but statistically shaped like prose: Zipf-distributed
pseudo-words of 1-3 common characters, sentences of 6-24 words, CJK
punctuation, occasional Latin words and numbers. Its zlib ratio (~2) is in
the range of real Chinese UTF-8 text. Run: python3 gen_data.py
"""
import json
import os
import random
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

# ~700 frequent simplified Chinese characters (frequency-ordered, roughly).
CHARS = (
    "的一是不了在人有我他这个们中来上大为和国地到以说时要就出会可也你对生能而子那得于着下自之年过发后作里用道"
    "行所然家种事成方多经么去法学如都同现当没动面起看定天分还进好小部其些主样理心她本前开但因只从想实日军者意"
    "无力它与长把机十民第公此已工使情明性知全三又关点正业外将两高间由问很最重并物手应战向头文体政美相见被利什"
    "二等产或新己制身果加西斯月话合回特代内信表化老给世位次度门任常先海通教儿原东声提立及比员解水名真论处走义"
    "各入几口认条平系气题活尔更别打女变四神总何电数安少报才结反受目太量再感建务做接必场件计管期市直德资命山金"
    "指克许统区保至队形社便空决治展马科司五基眼书非则听白却界达光放强即像难且权思王象完设式色路记南品住告类求"
    "据程北边死张该交规万取拉格望觉术领共确传师观清今切院让识候带导争运笑飞风步改收根干造言联持组每济车亲极林"
    "服快办议往元英士证近失转夫令准布始怎呢存未远叫台单影具罗字爱击流备兵连调深商算质团集百需价花党华城石级整"
    "府离况亚请技际约示复病息究线似官火断精满支视消越器容照须九增研写称企八功吗包片史委乎查轻易早曾除农找装广"
    "显吧阿李标谈吃图念六引历首医局突专费号尽另周较注语仅考落青随选列武红响虽推势参希古众构房半节土投某案黑维"
    "革划敌致陈律足态护七兴派孩验责营星够章音跟志底站严巴例防族供效续施留讲型料终答紧黄绝奇察母京段依批群项故"
    "按河米围江织害斗双境客纪采举杀攻父苏密低朝友诉止细愿千值仍男钱破网热助倒育属坐帝限船脸职速刻乐否刚威毛状"
    "率甚独球般普怕弹校苦创假久错承印晚兰试股拿脑预谁益阳若哪微尼继送急血惊伤素药适波夜省初喜卫源食险待述陆习"
    "置居劳财环排福纳欢雷警获模充负云停木游龙树疑层冷洲冲射略范竟句室异激汉村哈策演简卡罪判担州静退既衣您宗积"
    "余痛检差富灵协角占配征修皮挥胜降阶审沉坚善妈刘读啊超免压银买皇养伊怀执副乱抗犯追帮宣佛岁航优怪香著田铁控"
)
LATIN = ["AI", "E-Ink", "Wi-Fi", "LVGL", "ESP32", "app", "OK", "PDF", "USB-C", "4G", "GPU", "API", "Linux", "PSRAM"]
PUNCT_MID = "，，，，、、；："
PUNCT_END = "。。。。。？！"


def zipf_choice(rng, items, s=1.05):
    # inverse-CDF sampling over precomputed weights
    return items[min(len(items) - 1, int(len(items) * (rng.random() ** (1.0 + 3.0 * s))))]


def make_text(rng, target_bytes):
    words = []
    for _ in range(2600):
        n = rng.choice((1, 1, 2, 2, 2, 2, 3))
        words.append("".join(zipf_choice(rng, CHARS, 0.55) for _ in range(n)))
    out = []
    size = 0
    while size < target_bytes:
        para = ["　　"]
        for _ in range(rng.randint(2, 6)):
            sent = []
            for i in range(rng.randint(6, 24)):
                r = rng.random()
                if r < 0.025:
                    sent.append(" " + rng.choice(LATIN) + " ")
                elif r < 0.04:
                    sent.append(str(rng.randint(1, 2026)))
                else:
                    sent.append(zipf_choice(rng, words, 0.5))
                if i and rng.random() < 0.12:
                    sent.append(rng.choice(PUNCT_MID))
            s = "".join(sent).replace("  ", " ")
            if rng.random() < 0.08:
                s = "“" + s + "”"
            para.append(s + rng.choice(PUNCT_END))
        p = "".join(para) + "\n"
        out.append(p)
        size += len(p.encode("utf-8"))
    return "".join(out)


def make_json(rng, text):
    sents = [s for s in text.replace("　", "").replace("\n", "").split("。") if len(s) > 8]
    items = []
    for i in range(18):
        title = sents[rng.randrange(len(sents))][:22]
        summary = "。".join(sents[rng.randrange(len(sents))] for _ in range(rng.randint(2, 4))) + "。"
        if rng.random() < 0.4:
            summary += '\n“' + title[:6] + '” \\ "quoted"'
        items.append({
            "id": 1000000 + rng.randrange(900000),
            "title": title,
            "summary": summary,
            "url": "https://news.example.org/a/%08x" % rng.getrandbits(32),
            "source": {"name": rng.choice(["新华社", "央视新闻", "Reuters", "澎湃新闻"]), "id": rng.randrange(100)},
            "tags": [zipf_choice(rng, list(CHARS), 0.5) * 2 for _ in range(rng.randint(1, 4))],
            "score": round(rng.random() * 100, 3),
            "published": "2026-10-%02dT%02d:%02d:00+08:00" % (rng.randint(1, 31), rng.randint(0, 23), rng.randint(0, 59)),
            "read": rng.random() < 0.3,
            "image": None if rng.random() < 0.5 else {"w": 640, "h": 360, "url": "https://img.example.org/%d.jpg" % i},
        })
    doc = {"code": 0, "message": "ok", "data": {"page": 1, "total": 1873, "items": items}}
    return json.dumps(doc, ensure_ascii=False, separators=(",", ":"))


def main():
    rng = random.Random(20261102)
    text = make_text(rng, 16384)
    tb = text.encode("utf-8")
    z = zlib.compress(tb, 6)
    js = make_json(rng, text).encode("utf-8")
    with open(os.path.join(HERE, "text_zh.txt"), "wb") as f:
        f.write(tb)
    with open(os.path.join(HERE, "text_zh.zlib"), "wb") as f:
        f.write(z)
    with open(os.path.join(HERE, "sample.json"), "wb") as f:
        f.write(js)
    print("text_zh.txt %d bytes, %d chars; zlib %d bytes (ratio %.2f); sample.json %d bytes"
          % (len(tb), len(text), len(z), len(tb) / len(z), len(js)))


if __name__ == "__main__":
    main()
