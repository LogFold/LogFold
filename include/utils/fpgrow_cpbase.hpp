#ifndef LOGMD_FPGROW_HPP
#define LOGMD_FPGROW_HPP

#include "absl/container/flat_hash_map.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace std;

// FP 树节点类
class FPNode {
public:
  string item;
  int frequency;
  shared_ptr<FPNode> parent;
  shared_ptr<FPNode> next; // 指向相同项的下一个节点
  absl::flat_hash_map<string, shared_ptr<FPNode>> children;

  FPNode(const string &item, shared_ptr<FPNode> parent)
      : item(item), frequency(1), parent(parent), next(nullptr) {}
};

// FP 树类
class FPTree {
public:
  shared_ptr<FPNode> root;
  absl::flat_hash_map<string, shared_ptr<FPNode>> header_table;
  absl::flat_hash_map<string, int> item_frequency;
  int min_support;

  FPTree(int min_support) : min_support(min_support) {
    root = make_shared<FPNode>("", nullptr);
  }

  // 插入事务到 FP 树
  void insert_transaction(vector<string> &transaction) {
    shared_ptr<FPNode> current = root;

    for (auto &item : transaction) {
      // 查找子节点
      auto child = current->children.find(item);

      if (child != current->children.end()) {
        // 节点存在，增加频率
        child->second->frequency++;
        current = child->second;
      } else {
        // 创建新节点
        auto new_node = make_shared<FPNode>(item, current);
        current->children.emplace(item, new_node);
        // 更新项头表
        update_header_table(new_node);
        current = new_node;
      }
    }
  }

  // 获取条件模式基
  absl::flat_hash_map<vector<string>, int>
  get_conditional_pattern_base(const string &item) {
    absl::flat_hash_map<vector<string>, int> pattern_base;
    shared_ptr<FPNode> node = header_table[item];

    while (node) {
      // 获取前缀路径
      vector<string> path;
      shared_ptr<FPNode> parent = node->parent;

      while (parent && parent->item != "") {
        path.push_back(parent->item);
        parent = parent->parent;
      }

      if (!path.empty()) {
        reverse(path.begin(), path.end());
        pattern_base.emplace(move(path), node->frequency);
      }

      node = node->next;
    }

    return pattern_base;
  }

private:
  // 更新项头表
  void update_header_table(shared_ptr<FPNode> node) {
    const auto &item = node->item;

    if (header_table.find(item) == header_table.end()) {
      // header_table[item] = node;
      header_table.emplace(item, node);
    } else {
      shared_ptr<FPNode> current = header_table[item];
      // while (current->next) {
      //   current = current->next;
      // }
      node->next = current->next;
      current->next = node;
    }
  }
};

// FP-Growth 算法类
class FPGrowth {
public:
  FPGrowth(vector<vector<string>> &&transactions, int min_support)
      : transactions(transactions), min_support(min_support) {}
  //   FPGrowth(vector<pair<vector<string>, int>> &&transactions, int
  //   min_support)
  //       : transactions_with_counts(transactions), min_support(min_support),
  //         with_counts(true) {}

  // 运行算法
  vector<pair<vector<string>, int>> run() {

    FPTree fp_tree(min_support);

    // 1. 计算项频率
    // if (with_counts)
    //   calculate_frequency_with_counts(fp_tree.item_frequency);
    // else
    calculate_frequency(fp_tree.item_frequency);

    // 2. 过滤和排序事务
    vector<vector<string>> filtered_transactions;
    filter_and_sort(fp_tree.item_frequency, filtered_transactions);

    // 3. 构建 FP 树
    for (auto &transaction : filtered_transactions) {
      fp_tree.insert_transaction(transaction);
    }

    // 4. 挖掘频繁项集
    return mine_fp_tree(fp_tree);
  }

private:
  vector<vector<string>> transactions;
  // vector<pair<vector<string>, int>> transactions_with_counts;
  int min_support;

  // 计算全局项频率
  void calculate_frequency(absl::flat_hash_map<string, int> &frequency) {
    for (auto &transaction : transactions) {
      for (auto &item : transaction) {
        frequency[item]++;
      }
    }
  }

  //   void
  //   calculate_frequency_with_counts(absl::flat_hash_map<string, int>
  //   &frequency) {
  //     for (auto &[transaction, count] : transactions_with_counts) {
  //       for (auto &item : transaction) {
  //         frequency[item] += count;
  //       }
  //     }
  //   }

  // 过滤非频繁项并排序
  void filter_and_sort(absl::flat_hash_map<string, int> &frequency,
                       vector<vector<string>> &result) {
    result.reserve(transactions.size());
    for (auto &transaction : transactions) {

      vector<string> filtered;

      for (auto &item : transaction) {
        if (frequency[item] >= min_support) {
          filtered.push_back(item);
        }
      }

      if (filtered.empty())
        continue;

      // 按频率降序排序
      sort(filtered.begin(), filtered.end(),
           [&](const string &a, const string &b) {
             auto &fa = frequency[a], fb = frequency[b];
             return fa != fb ? fa > fb : a < b;
           });

      result.emplace_back(move(filtered));
    }
  }

  //   void filter_and_sort_with_counts(absl::flat_hash_map<string, int>
  //   &frequency,
  //                                    vector<pair<vector<string>, int>>
  //                                    &result) {
  //     result.reserve(transactions_with_counts.size());
  //     result.push_back({{}, 0});
  //     for (auto &[transaction, count] : transactions_with_counts) {
  //       if (!result.back().first.empty()) {
  //         result.push_back({{}, 0});
  //       }
  //       auto &filtered = result.back();

  //       for (string &item : transaction) {
  //         if (frequency[item] >= min_support) {
  //           filtered.first.push_back(item);
  //           filtered.second += count;
  //         }
  //       }

  //       if (filtered.first.empty())
  //         continue;

  //       // 按频率降序排序
  //       sort(filtered.first.begin(), filtered.first.end(),
  //            [&](const string &a, const string &b) {
  //              auto &fa = frequency[a], fb = frequency[b];
  //              return fa != fb ? fa > fb : a < b;
  //            });
  //     }
  //   }

  // 递归挖掘 FP 树
  vector<pair<vector<string>, int>> mine_fp_tree(FPTree &fp_tree) {
    vector<pair<vector<string>, int>> patterns;
    patterns.reserve(fp_tree.item_frequency.size());
    // 处理频繁1项集
    for (auto &[item, frequency] : fp_tree.item_frequency) {
      if (frequency >= min_support) {
        auto pattern = vector<string>{item};
        patterns.emplace_back(move(pattern), frequency);
      }
    }

    // 对每个频繁项递归挖掘
    for (auto &[item, node] : fp_tree.header_table) {
      // 获取条件模式基
      auto pattern_base = fp_tree.get_conditional_pattern_base(item);

      // 构建条件 FP 树
      //   FPTree conditional_tree(min_support);
      //   absl::flat_hash_map<string, int> conditional_frequency;

      //   for (auto &[path, count] : pattern_base) {
      //     for (auto &path_item : path) {
      //       conditional_frequency[path_item] += count;
      //     }
      //   }

      // 过滤非频繁项
      //   for (auto it = conditional_frequency.begin();
      //        it != conditional_frequency.end();) {
      //     if (it->second < min_support) {
      //       it = conditional_frequency.erase(it);
      //     } else {
      //       ++it;
      //     }
      //   }

      // 构建条件事务
      vector<vector<string>> conditional_transactions;
      for (auto &[path, count] : pattern_base) {
        size_t len = conditional_transactions.size();
        conditional_transactions.resize(len + count, path);
      }

      // 递归挖掘条件 FP 树
      if (!conditional_transactions.empty()) {
        FPGrowth fp_growth(move(conditional_transactions), min_support);
        auto conditional_patterns = fp_growth.run();

        // 合并模式
        patterns.reserve(patterns.size() + conditional_patterns.size());
        for (auto &[pattern, frequency] : conditional_patterns) {
          vector<string> new_pattern = pattern;
          new_pattern.push_back(item);
          sort(new_pattern.begin(), new_pattern.end());
          patterns.emplace_back(move(new_pattern), frequency);
        }
      }
    }

    return patterns;
  }
};

#endif // LOGMD_FPGROW_HPP