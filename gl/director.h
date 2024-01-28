#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader_program.h"

namespace tc
{

	class Director {
	public:

		static std::shared_ptr<Director> Make(GLFunctions* fk);

		Director(GLFunctions* fk);
		~Director();

		void Init(int width, int height);

		glm::mat4 GetProjection();
		glm::mat4 GetView();

		GLFunctions* Funcs();

	private:

		glm::mat4 projection;
		glm::mat4 view;

		float width = 0.0f;
		float height = 0.0f;

		GLFunctions* funcs = nullptr;

	};

}