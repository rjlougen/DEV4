#pragma once
#include <fstream>
#include "h2bParser.h"

class ModelImporter {
public:
	std::vector<GameLevel> LoadGameLevel(std::string filename) {
		std::ifstream infile(filename);

		std::vector<GameLevel> returnLevel;

		std::string line;
		std::string type = "";
		std::string modelName = "";
		unsigned int matrixLine = 0;
		GameLevel tempLevel;

		while (std::getline(infile, line))
		{
			std::istringstream iss(line);

			//Ignore commented lines
			if (line.substr(0,1) == "#") {
				continue;
			}

			if (type.size() == 0) {
				if (line.compare("MESH") == 0) {
					type = "MESH";
					tempLevel.type = "MESH";
					continue;
				}
			}

			if (type == "MESH") {
				// Process name
				if (modelName.size() == 0) {
					modelName = line;
					tempLevel.modelName = line;
					continue;
				}

				line.erase(remove_if(line.begin(), line.end(), isspace), line.end());

				// Process matrix
				if (line.substr(0, 10) == "<Matrix4x4") {
					tempLevel.worldMatrix.row1 = ParseLine(line.substr(10, line.size()));
					matrixLine++;
					continue;
				}
				else {
					switch (matrixLine)
					{
					case 1:
						tempLevel.worldMatrix.row2 = ParseLine(line);
						break;
					case 2:
						tempLevel.worldMatrix.row3 = ParseLine(line);
						break;
					case 3:
						tempLevel.worldMatrix.row4 = ParseLine(line);
						break;
					default:
						break;
					}
					matrixLine++;
					if (matrixLine >= 4) {
						type = "";
						modelName = "";
						matrixLine = 0;
						returnLevel.push_back(tempLevel);
						continue;
					}
				}
				continue;
			}

			printf("%s\n", line.c_str());
		}
		return returnLevel;
	}

	GW::MATH::GVECTORF ParseLine(std::string line) {
		GW::MATH::GVECTORF row;
		std::string remaining = line.substr(1, line.size());
		int matrixCol = 0;
		while (matrixCol < 4) {
			std::string num = "";
			for (char& c : remaining) {
				if (c == ',' || c == ')') {
					break;
				}
				num += c;
			}
			remaining.replace(0, num.size() + 1, "");
			switch (matrixCol)
			{
			case 0:
				row.x = std::stof(num);
				break;
			case 1:
				row.y = std::stof(num);
				break;
			case 2:
				row.z = std::stof(num);
				break;
			case 3:
				row.w = std::stof(num);
				break;
			default:
				break;
			}
			matrixCol++;
		}
		return row;
	}

	H2B::Parser LoadOBJ(std::string filename) {
		H2B::Parser objParse;
		objParse.Parse(filename.c_str());

		return objParse;
	}
};