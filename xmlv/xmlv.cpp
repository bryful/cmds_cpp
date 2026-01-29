#include "pugixml.hpp"
#include <iostream>
#include <string>

// ノードの内容を再帰的に表示する関数
void print_node(pugi::xml_node node, int indent = 0) {
    // インデントの作成
    std::string shift(indent, ' ');

    // ノードの種類に応じて処理を分岐
    if (node.type() == pugi::node_element) {
        std::cout << shift << "Element: " << node.name();

        // 属性があれば表示
        for (pugi::xml_attribute attr : node.attributes()) {
            std::cout << " [" << attr.name() << " = " << attr.value() << "]";
        }
        std::cout << std::endl;
    }
    else if (node.type() == pugi::node_pcdata) {
        // テキストデータの場合
        if (std::string(node.value()).find_first_not_of(" \n\r\t") != std::string::npos) {
            std::cout << shift << "Text: " << node.value() << std::endl;
        }
    }

    // 子ノードに対して再帰呼び出し
    for (pugi::xml_node child : node.children()) {
        print_node(child, indent + 2);
    }
}

int main() {
    pugi::xml_document doc;

    // XMLファイルのロード
    pugi::xml_parse_result result = doc.load_file("data.xml");

    if (!result) {
        std::cerr << "XML読み込み失敗: " << result.description() << std::endl;
        return 1;
    }

    std::cout << "--- XML Structure ---" << std::endl;
    print_node(doc);

    return 0;
}