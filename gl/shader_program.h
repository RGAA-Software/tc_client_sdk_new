#pragma once

#include <memory>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef WIN32
#include <QOpenGLFunctions_3_3_Core>
#endif

#ifdef ANDROID
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2ext.h>
#endif

namespace tc
{

	class GLFunctions {
	public:

#ifdef WIN32
		QOpenGLFunctions_3_3_Core* core_ = nullptr;
#endif

	};

#ifdef WIN32
#define GL_FUNC this->fk_->core_->
#endif

#ifdef ANDROID
#define GL_FUNC
#endif

	class ShaderProgram {

	public:

		static std::shared_ptr<ShaderProgram> Make(const GLFunctions* fk, const std::string& vertex, const std::string& fragment);

		ShaderProgram(const GLFunctions* fk, const std::string& vertex, const std::string& fragment);
		~ShaderProgram();

		void Active();
		int GetProgramId();

		void Release() {
			GL_FUNC glUseProgram(0);
		}

		int GetAttribLocation(const std::string& name) {
			return GL_FUNC glGetAttribLocation(program_id_, name.c_str());
		}

		int GetUniformLocation(const std::string& name) {
			return GL_FUNC glGetUniformLocation(program_id_, name.c_str());
		}

		void SetUniform1i(const std::string& name, int value) {
			GL_FUNC glUniform1i(GetUniformLocation(name), value);
		}

		void SetUniform1f(const std::string& name, float value) {
			GL_FUNC glUniform1f(GetUniformLocation(name), value);
		}

		void SetUniform2fv(const std::string& name, const glm::vec2& vec) {
			GL_FUNC glUniform2f(GetUniformLocation(name), vec.x, vec.y);
		}

		void SetUniform3fv(const std::string& name, const glm::vec3& vec) {
			GL_FUNC glUniform3f(GetUniformLocation(name), vec.x, vec.y, vec.z);
		}

		void SetUniformMatrix(const std::string& name, const glm::mat4& m) {
			auto loc = GetUniformLocation(name);
			GL_FUNC glUniformMatrix4fv(loc, 1, false, glm::value_ptr(m));
		}

		static void CheckShaderError(GLuint id, const std::string& type);

	private:

		GLuint program_id_;
		GLFunctions* fk_;

	};

	typedef std::shared_ptr<ShaderProgram> ShaderProgramPtr;
}