# Google Pinyin 来源与适配

来源：[LineageOS/PinyinIME](https://github.com/LineageOS/android_packages_inputmethods_PinyinIME/tree/a08d08cdea4319fb74b55289e1b4e1b1a5249bc2/jni)，查阅日期 2026-11-12。

`a08d08cdea4319fb74b55289e1b4e1b1a5249bc2` 与 `1702573e` 的 `jni/` 完全相同。保留原 NOTICE（本目录 LICENSE，含 Apache-2.0 全文）和文件头。十二个运行文件完整移植，未引入建库工具、Java 和同步入口；dictbuilder/spellingtable 仅带编译依赖的头文件。

词库 `assets/ime/dict_pinyin.dat`：1,068,442 字节，SHA-256 `6bf0bbde4e3134cce38d08524a9f4dc1af40435243c8e36b38eb68d1e14462b2`。未放入构建资源包；使用时放存储卡 `/content/ime/dict_pinyin.dat`。

## 文件哈希

| 文件 | 上游 SHA-256 | 入库 SHA-256 |
| --- | --- | --- |
| `include/atomdictbase.h` | `04ec8f2d51b07f5d309bab4d5eed3dcd4f8ce458d3defe4162614e10ff424f0a` | `04ec8f2d51b07f5d309bab4d5eed3dcd4f8ce458d3defe4162614e10ff424f0a` |
| `include/dictbuilder.h` | `35941652c4419458805e30bb9d89a0b69cdd7a00b4a4b895c1e47acdd347dcba` | `35941652c4419458805e30bb9d89a0b69cdd7a00b4a4b895c1e47acdd347dcba` |
| `include/dictdef.h` | `beea8f3c38583ed65dd07d284f92245eb589261854f8da0b4f2f41df69c440ae` | `5321129227c9c12dc28dd4a10f243a264358d67a4ec71a47499a18fca0be5d33` |
| `include/dictlist.h` | `2a73af24549b8c37951fe2c4f633acfd2bf824e49f1c4541b13d92bb356dd703` | `2a73af24549b8c37951fe2c4f633acfd2bf824e49f1c4541b13d92bb356dd703` |
| `include/dicttrie.h` | `1fc01e46fed0dbf89c63940490db331f7008fc5e207e3cf67912c3714bb80c05` | `1fc01e46fed0dbf89c63940490db331f7008fc5e207e3cf67912c3714bb80c05` |
| `include/lpicache.h` | `be5cbf7a69c7ab12b6912b226a3dcbde9f52e288c6639b14b4abe2df0a35756d` | `be5cbf7a69c7ab12b6912b226a3dcbde9f52e288c6639b14b4abe2df0a35756d` |
| `include/matrixsearch.h` | `ff8719a86e33c9f20c98ff407b2f80ada5f6363cf7baf24827a1f2549a7834b8` | `ff8719a86e33c9f20c98ff407b2f80ada5f6363cf7baf24827a1f2549a7834b8` |
| `include/mystdlib.h` | `c5b8f130dd1eb8d49264c97b46f65bb9643bdbbce76faf27dfd4ccb3a8a327de` | `c5b8f130dd1eb8d49264c97b46f65bb9643bdbbce76faf27dfd4ccb3a8a327de` |
| `include/ngram.h` | `5901a426b3f5b2ca93b603fa80733801f582057444f3aaa45267b0b12bb27188` | `5901a426b3f5b2ca93b603fa80733801f582057444f3aaa45267b0b12bb27188` |
| `include/pinyinime.h` | `44b4693855433f41662d8af0e516dc7c9ce6b06d72c6610e370bb2c4c25b7015` | `44b4693855433f41662d8af0e516dc7c9ce6b06d72c6610e370bb2c4c25b7015` |
| `include/searchutility.h` | `4ddbdf54bdf70220504d8d4a783f6e19c4bef8e5e0fc9a8ee1307b7811e9dfa4` | `4ddbdf54bdf70220504d8d4a783f6e19c4bef8e5e0fc9a8ee1307b7811e9dfa4` |
| `include/spellingtable.h` | `69c1b45651156a68177372098661b658d630a398395dc614fb2162ca8883d3a1` | `69c1b45651156a68177372098661b658d630a398395dc614fb2162ca8883d3a1` |
| `include/spellingtrie.h` | `0912d88ed87118b8b2c263851800c9f5dd04aa81f903bd00eb2a081c884dbcc7` | `0912d88ed87118b8b2c263851800c9f5dd04aa81f903bd00eb2a081c884dbcc7` |
| `include/splparser.h` | `09a1daaec9366ebe4202d69c54a9b93534791fc976eaba5f313c209c82193f0b` | `09a1daaec9366ebe4202d69c54a9b93534791fc976eaba5f313c209c82193f0b` |
| `include/userdict.h` | `3710026e255998961a5eece771635b682401b8fbb0f860cc8c1f0b77295537d5` | `3710026e255998961a5eece771635b682401b8fbb0f860cc8c1f0b77295537d5` |
| `include/utf16char.h` | `90bf7e256ad0c050f59a22acdd79a9eaebe0de32aae9684ed7b68e44cdbd6126` | `90bf7e256ad0c050f59a22acdd79a9eaebe0de32aae9684ed7b68e44cdbd6126` |
| `share/dictlist.cpp` | `fa95e96cac5e28a8b3a254de36df06cc8936795a6b93163139d1b37fb333efd3` | `b32d5a17af59b08da8f83f1c9e06fd7b37caa2d88e2cf471519a8cf55968efdf` |
| `share/dicttrie.cpp` | `b5da1225d9d941844396e9d7dacd825e757fbe7c1c9097ac674468e4be21c32e` | `3aa127a42cfac23f68e0700d3306034f55b83c055302b6a313c5a2d0ba439d2a` |
| `share/lpicache.cpp` | `b61f8175d834a976bde9e8faac8d074784b12011429ce4ad48bdb24fd9efbd47` | `8b1b9bbaa4a97eda15f7106aa50cd00ff186b59b4bbf45a16ad81bb78b82cc38` |
| `share/matrixsearch.cpp` | `1cb744bd5d4ea9851586e3fd08940cf443786c0c259380043fb73b104b70524d` | `4d2ae74051a353f67185c88b589ca87d3dea46d3d6152ca256ac06bdf4026867` |
| `share/mystdlib.cpp` | `753725a943da62462feeaa33d9f7c0c2aa1083bfc7b256299663b63567760c37` | `753725a943da62462feeaa33d9f7c0c2aa1083bfc7b256299663b63567760c37` |
| `share/ngram.cpp` | `070bcdec1128ec056f71b788b7a6c77f3421f230267cd94fda9f224ae572c5aa` | `55dea25cf1855070eb27457d31e2693eb58ad8f54715bb254be8b8ab18bb5780` |
| `share/pinyinime.cpp` | `512e6716d77d63ab8ad5e4cd2372430b00a4f0c6e34b9ff0a797a96e578f4cf9` | `10fc92abc7bc020fe7815f16e54688d68b026b99773fbd9e7897041dcc19ed16` |
| `share/searchutility.cpp` | `72b5b4de7065b675cd7d2dd50e2ece9a25cded2d4359865e943ae832994f7d73` | `72b5b4de7065b675cd7d2dd50e2ece9a25cded2d4359865e943ae832994f7d73` |
| `share/spellingtrie.cpp` | `5df7f65cbe4f63e84e182d9ba2af35227327c8268b057bdf0aec204ca2a05f60` | `adcc012ff983e4f00ed5b533eedf9961184f9332faa43289bd85c9b05154b9c8` |
| `share/splparser.cpp` | `33c150844d15e36474e3da12eb2d36efd23645a658d49f8ebaf529bf1223b03d` | `33c150844d15e36474e3da12eb2d36efd23645a658d49f8ebaf529bf1223b03d` |
| `share/userdict.cpp` | `dab022028d2f299a03f697a1015a4e391f1371877ab4dcfd7b7962b9624ecda6` | `2cb67fc1cd45afcba3b7da5973e5c7b98c2aff8d9947ff50b6339b69f26abad5` |
| `share/utf16char.cpp` | `bd3da194b3b694c2dd4c44553d4602253014948746dd50810759b8a5557de9dc` | `bd3da194b3b694c2dd4c44553d4602253014948746dd50810759b8a5557de9dc` |

## 适配理由

- 文件里的 size_t 读写改 uint32 转换；LE0 节点的两个磁盘偏移改 uint32。内存索引保持 size_t，适用项目的 little-endian 目标。
- new/delete、malloc/realloc/free/strdup 接 OSAL，全部动态缓冲放 PSRAM。私有单例构造保留原调用点的 placement new。
- 仅对原文预包含 `google_port.h`：文件接成员文件服务和存储 HAL，用户文件原子写回，锁和时间接 OSAL，Android 日志禁用。不使用的 Android fd 打开入口返回失败。
- 用户词库上限 512 条、词条区 16 KiB，回收 20%；适配器的 64 KiB 上限含索引和频率，失败保留旧文件。
- OSAL 时钟可能在同一微秒完成操作，允许 flush 的更新时间相等。
- 重开时回收旧拼音树、词条索引、同步数组，避免换人后 PSRAM 持续增长；修正遗漏的数组 delete，统一走 OSAL。
- 预测静态缓冲放 PSRAM。上游原文编译使用 -w；胶水保留工程警告检查。

## 逐段改动

共 127 个行位置；每段按新旧行数的最大值计。

- `include/dictdef.h:112`（上游 112–113）：uint32 son_1st_off;；uint32 homo_idx_buf_off;
- `share/dictlist.cpp:387`（上游 387–390）：if (ink_gp_write_32(&scis_num_, 1, fp) != 1)；return false;；；if (ink_gp_write_32(start_pos_, kMaxLemmaSize + 1, fp) !=
- `share/dictlist.cpp:394`（上游 394–394）：if (ink_gp_write_32(start_id_, kMaxLemmaSize + 1, fp) !=
- `share/dictlist.cpp:417`（上游 417–420）：if (ink_gp_read_32(&scis_num_, 1, fp) != 1)；return false;；；if (ink_gp_read_32(start_pos_, kMaxLemmaSize + 1, fp) !=
- `share/dictlist.cpp:424`（上游 424–424）：if (ink_gp_read_32(start_id_, kMaxLemmaSize + 1, fp) !=
- `share/dicttrie.cpp:64`（上游 64–66）：if (NULL != lma_idx_buf_)；free(lma_idx_buf_);；lma_idx_buf_ = NULL;
- `share/dicttrie.cpp:70`（上游 70–70）：ink_gp_delete(dict_list_);
- `share/dicttrie.cpp:76`（上游 76–76）：ink_gp_delete(parsing_marks_);
- `share/dicttrie.cpp:80`（上游 80–80）：ink_gp_delete(mile_stones_);
- `share/dicttrie.cpp:105`（上游 105–105）：DictBuilder* dict_builder = ink_gp_new<DictBuilder>();
- `share/dicttrie.cpp:116`（上游 116–125）：if (ink_gp_write_32(&lma_node_num_le0_, 1, fp) != 1)；return false;；；if (ink_gp_write_32(&lma_node_num_ge1_, 1, fp) != 1)；return false;；；if (ink_gp_write_32(&lma_idx_buf_len_, 1, fp) != 1)；return false;；；if (ink_gp_write_32(&top_lmas_num_, 1, fp) != 1)
- `share/dicttrie.cpp:172`（上游 172–181）：if (ink_gp_read_32(&lma_node_num_le0_, 1, fp) != 1)；return false;；；if (ink_gp_read_32(&lma_node_num_ge1_, 1, fp) != 1)；return false;；；if (ink_gp_read_32(&lma_idx_buf_len_, 1, fp) != 1)；return false;；；if (ink_gp_read_32(&top_lmas_num_, 1, fp) != 1 ||
- `share/dicttrie.cpp:199`（上游 199–200）：parsing_marks_ = ink_gp_new<ParsingMark>(kMaxParsingMark);；mile_stones_ = ink_gp_new<MileStone>(kMaxMileStone);
- `share/dicttrie.cpp:255`（上游 255–255）：dict_list_ = ink_gp_new<DictList>();
- `share/dicttrie.cpp:293`（上游 293–293）：dict_list_ = ink_gp_new<DictList>();
- `share/lpicache.cpp:25`（上游 25–26）：lpi_cache_ = ink_gp_new<LmaPsbItem>(kFullSplIdStart * kMaxLpiCachePerId);；lpi_cache_len_ = ink_gp_new<uint16>(kFullSplIdStart);
- `share/lpicache.cpp:35`（上游 35–35）：ink_gp_delete(lpi_cache_);
- `share/lpicache.cpp:38`（上游 38–38）：ink_gp_delete(lpi_cache_len_);
- `share/lpicache.cpp:43`（上游 43–43）：instance_ = new (ink_gp_alloc(sizeof(LpiCache))) LpiCache();
- `share/matrixsearch.cpp:73`（上游 73–75）：dict_trie_ = ink_gp_new<DictTrie>();；user_dict_ = static_cast<AtomDictBase*>(ink_gp_new<UserDict>());；spl_parser_ = ink_gp_new<SpellingParser>();
- `share/matrixsearch.cpp:87`（上游 87–87）：share_buf_ = ink_gp_new<size_t>(mtrx_nd_size + dmi_size + matrix_size + dep_size);
- `share/matrixsearch.cpp:109`（上游 109–109）：ink_gp_delete(dict_trie_);
- `share/matrixsearch.cpp:112`（上游 112–112）：ink_gp_delete(user_dict_);
- `share/matrixsearch.cpp:115`（上游 115–115）：ink_gp_delete(spl_parser_);
- `share/matrixsearch.cpp:118`（上游 118–118）：ink_gp_delete(share_buf_);
- `share/matrixsearch.cpp:136`（上游 136–136）：ink_gp_delete(user_dict_);
- `share/matrixsearch.cpp:139`（上游 139–138）：static_cast<UserDict*>(user_dict_)->set_limit(512, 16384, 20);
- `share/matrixsearch.cpp:161`（上游 160–160）：ink_gp_delete(user_dict_);
- `share/matrixsearch.cpp:164`（上游 163–162）：static_cast<UserDict*>(user_dict_)->set_limit(512, 16384, 20);
- `share/ngram.cpp:78`（上游 78–78）：size_t *item_num =  ink_gp_new<size_t>(kCodeBookSize);
- `share/ngram.cpp:82`（上游 82–82）：double *cb_new = ink_gp_new<double>(kCodeBookSize);
- `share/ngram.cpp:98`（上游 98–99）：ink_gp_delete(item_num);；ink_gp_delete(cb_new);
- `share/ngram.cpp:156`（上游 156–156）：instance_ = new (ink_gp_alloc(sizeof(NGram))) NGram();
- `share/ngram.cpp:167`（上游 167–167）：if (ink_gp_write_32(&idx_num_, 1, fp) != 1)
- `share/ngram.cpp:186`（上游 186–186）：if (ink_gp_read_32(&idx_num_, 1, fp) != 1 )
- `share/ngram.cpp:250`（上游 250–250）：double *freqs = ink_gp_new<double>(next_idx_unused);
- `share/ngram.cpp:284`（上游 284–284）：freq_codes_df_ = ink_gp_new<double>(kCodeBookSize);
- `share/ngram.cpp:289`（上游 289–289）：freq_codes_ = ink_gp_new<LmaScoreType>(kCodeBookSize);
- `share/ngram.cpp:316`（上游 316–316）：lma_freq_idx_ = ink_gp_new<CODEBOOK_TYPE>(idx_num_);
- `share/ngram.cpp:321`（上游 321–321）：ink_gp_delete(freqs);
- `share/pinyinime.cpp:35`（上游 35–35）：char16 predict_buf[kMaxPredictNum][kMaxPredictSize + 1] INK_PSRAM_BSS;
- `share/pinyinime.cpp:39`（上游 39–39）：ink_gp_delete(matrix_search);
- `share/pinyinime.cpp:41`（上游 41–41）：matrix_search = ink_gp_new<MatrixSearch>();
- `share/pinyinime.cpp:52`（上游 52–52）：ink_gp_delete(matrix_search);
- `share/pinyinime.cpp:54`（上游 54–54）：matrix_search = ink_gp_new<MatrixSearch>();
- `share/pinyinime.cpp:64`（上游 64–64）：ink_gp_delete(matrix_search);
- `share/spellingtrie.cpp:78`（上游 78–78）：ink_gp_delete(spelling_buf_);
- `share/spellingtrie.cpp:81`（上游 81–81）：ink_gp_delete(splstr_queried_);
- `share/spellingtrie.cpp:84`（上游 84–84）：ink_gp_delete(splstr16_queried_);
- `share/spellingtrie.cpp:87`（上游 87–87）：ink_gp_delete(spl_ym_ids_);
- `share/spellingtrie.cpp:91`（上游 91–91）：ink_gp_delete(root_);
- `share/spellingtrie.cpp:95`（上游 95–95）：ink_gp_delete(dumb_node_);
- `share/spellingtrie.cpp:99`（上游 99–99）：ink_gp_delete(splitter_node_);
- `share/spellingtrie.cpp:103`（上游 103–103）：ink_gp_delete(instance_);
- `share/spellingtrie.cpp:108`（上游 108–108）：ink_gp_delete(ym_buf_);
- `share/spellingtrie.cpp:111`（上游 111–111）：ink_gp_delete(f2h_);
- `share/spellingtrie.cpp:233`（上游 233–233）：instance_ = new (ink_gp_alloc(sizeof(SpellingTrie))) SpellingTrie();
- `share/spellingtrie.cpp:269`（上游 269–269）：ink_gp_delete(node->first_son);
- `share/spellingtrie.cpp:286`（上游 286–287）：ink_gp_delete(spelling_buf_);；spelling_buf_ = ink_gp_new<char>(item_size * item_num);
- `share/spellingtrie.cpp:300`（上游 300–301）：ink_gp_delete(splstr_queried_);；splstr_queried_ = ink_gp_new<char>(spelling_size_);
- `share/spellingtrie.cpp:306`（上游 306–307）：ink_gp_delete(splstr16_queried_);；splstr16_queried_ = ink_gp_new<char16>(spelling_size_);
- `share/spellingtrie.cpp:318`（上游 318–318）：if (root_) { free_son_trie(root_); ink_gp_delete(root_); }；ink_gp_delete(dumb_node_);；ink_gp_delete(splitter_node_);；root_ = ink_gp_new<SpellingNode>();
- `share/spellingtrie.cpp:324`（上游 321–321）：dumb_node_ = ink_gp_new<SpellingNode>();
- `share/spellingtrie.cpp:328`（上游 325–325）：splitter_node_ = ink_gp_new<SpellingNode>();
- `share/spellingtrie.cpp:372`（上游 369–369）：SpellingTable *spl_table = ink_gp_new<SpellingTable>();
- `share/spellingtrie.cpp:392`（上游 389–390）：ink_gp_delete(ym_buf_);；ym_buf_ = ink_gp_new<char>(ym_item_size * ym_num);
- `share/spellingtrie.cpp:395`（上游 392–392）：ink_gp_delete(spl_table);
- `share/spellingtrie.cpp:403`（上游 400–400）：ink_gp_delete(spl_table);
- `share/spellingtrie.cpp:407`（上游 404–405）：ink_gp_delete(spl_ym_ids_);；spl_ym_ids_ = ink_gp_new<uint8>(spelling_num_ + kFullSplIdStart);
- `share/spellingtrie.cpp:459`（上游 456–456）：first_son = ink_gp_new<SpellingNode>(num_of_son);
- `share/spellingtrie.cpp:646`（上游 643–646）：if (ink_gp_write_32(&spelling_size_, 1, fp) != 1)；return false;；；if (ink_gp_write_32(&spelling_num_, 1, fp) != 1)
- `share/spellingtrie.cpp:669`（上游 666–669）：if (ink_gp_read_32(&spelling_size_, 1, fp) != 1)；return false;；；if (ink_gp_read_32(&spelling_num_, 1, fp) != 1)
- `share/spellingtrie.cpp:682`（上游 679–681）：ink_gp_delete(spelling_buf_);；；spelling_buf_ = ink_gp_new<char>(spelling_size_ * spelling_num_);
- `share/spellingtrie.cpp:698`（上游 695–696）：ink_gp_delete(f2h_);；f2h_ = ink_gp_new<uint16>(spelling_num_);
- `share/userdict.cpp:23`（上游 23–23）：// Android logging is disabled by the port.
- `share/userdict.cpp:298`（上游 298–298）：load_time_.tv_usec >= g_last_update_.tv_usec)) {
- `share/userdict.cpp:319`（上游 319–318）：free(syncs_);
- `share/userdict.cpp:1730`（上游 1729–1729）：SpellingParser * spl_parser = ink_gp_new<SpellingParser>();
