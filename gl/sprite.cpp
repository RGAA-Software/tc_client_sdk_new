#include "sprite.h"

#include <iostream>

#include "director.h"
#include "raw_image.h"
#include "shader_program.h"
#include "video_widget_shaders.h"

namespace tc
{

	Sprite::Sprite(const std::shared_ptr<Director>& director) : Renderer(director) {
		Init();
	}

	Sprite::~Sprite() {
	
	}

	void Sprite::Init() {
		GL_FUNC glGenVertexArrays(1, &render_vao);
		GL_FUNC glBindVertexArray(render_vao);

		shader_program = ShaderProgram::Make(functions, kOperationVertexShader, kRGBAFragmentShader);

		float vertices[] = {
			0.0f, 0.0f, 0.0f, 1.0f, 0, 0,  0, 0,
			80.0f, 0.0f, 0.0f, 0, 1.0f, 0,  1, 0,
			80.0f,  80.0f, 0.0f, 0, 0, 1.0f,  1, 1,
			0.0f, 80.0f, 0.0f, 1.0f, 1.0f, 0, 0, 1
		};

		int indices[] = {
			0, 1, 2,
			2, 3, 0
		};

		GL_FUNC glGenBuffers(1, &vbo);
		GL_FUNC glBindBuffer(GL_ARRAY_BUFFER, vbo);
		GL_FUNC glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		int posLoc = GL_FUNC glGetAttribLocation(shader_program->GetProgramId(), "aPos");
		GL_FUNC glVertexAttribPointer(posLoc, 3, GL_FLOAT, false, 8 * 4, 0);
		GL_FUNC glEnableVertexAttribArray(posLoc);

		int colorLoc = GL_FUNC glGetAttribLocation(shader_program->GetProgramId(), "aColor");
		GL_FUNC glVertexAttribPointer(colorLoc, 3, GL_FLOAT, false, 8 * 4, (void*)(3 * 4));
		GL_FUNC glEnableVertexAttribArray(colorLoc);

		int texLoc = GL_FUNC glGetAttribLocation(shader_program->GetProgramId(), "aTex");
		GL_FUNC glVertexAttribPointer(texLoc, 2, GL_FLOAT, false, 8 * 4, (void*)(6 * 4));
		GL_FUNC glEnableVertexAttribArray(texLoc);

		GLuint ibo;
		GL_FUNC glGenBuffers(1, &ibo);
		GL_FUNC glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		GL_FUNC glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		GL_FUNC glGenTextures(1, &texture_id);
		GL_FUNC glBindTexture(GL_TEXTURE_2D, texture_id);
		GL_FUNC glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GL_FUNC glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		GL_FUNC glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		GL_FUNC glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//GL_FUNC glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		//GL_FUNC glGenerateMipmap(GL_TEXTURE_2D);

		//std::cout << "shader_program : " << shader_program->GetProgramId() << " texLoc : " << texLoc << std::endl;
		GL_FUNC glBindVertexArray(0);
	}

	void Sprite::UpdateImage(std::shared_ptr<RawImage> image) {
		std::lock_guard<std::mutex> guard(buf_mtx);
		this->image = image;
	}

	void Sprite::ForceImageSize(int width, int height) {
		force_width = width;
		force_height = height;
	}

	void Sprite::UpdateTranslation(int x, int y) {
		trans_x = x;
		trans_y = y;
	}

	void Sprite::UpdateTranslationPercentWindow(float x, float y) {
		trans_xf = x;
		trans_yf = y;
	}

	void Sprite::UpdateTranslationAdjuster(float x, float y) {
		adjuster_x = x;
		adjuster_y = y;
	}

	void Sprite::Render(float delta) {
		{
			std::lock_guard<std::mutex> guard(buf_mtx);
			if (!image) {
				return;
			}
		}

		GL_FUNC glBindVertexArray(render_vao);
		shader_program->Active();

		GL_FUNC glEnable(GL_BLEND);
		GL_FUNC glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(window_width * trans_xf + adjuster_x, window_height * (1.0f - trans_yf) + adjuster_y, 0));
		shader_program->SetUniformMatrix("model", model);
		shader_program->SetUniformMatrix("view", director->GetView());
		shader_program->SetUniformMatrix("projection", director->GetProjection());

		int target_width = force_width > 0 ? force_width : image->img_width;
		int target_height = force_height > 0 ? force_height : image->img_height;

		float vertices[] = {
			0.0f, 0.0f, 0.0f, 1.0f, 0, 0,  0, 0,
			target_width * 1.0f, 0.0f, 0.0f, 0, 1.0f, 0,  1, 0,
			target_width * 1.0f, -target_height*1.0f, 0.0f, 0, 0, 1.0f,  1, 1,
			0.0f, -target_height *1.0f, 0.0f, 1.0f, 1.0f, 0, 0, 1
		};

		GL_FUNC glBindBuffer(GL_ARRAY_BUFFER, vbo);
		GL_FUNC glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		GL_FUNC glActiveTexture(GL_TEXTURE0);
		GL_FUNC glBindTexture(GL_TEXTURE_2D, texture_id);
		GL_FUNC glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->img_width, image->img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->Data());
		GL_FUNC glUniform1i(GL_FUNC glGetUniformLocation(shader_program->GetProgramId(), "image1"), 0);
		GL_FUNC glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		GL_FUNC glBindVertexArray(0);

        GL_FUNC glDisable(GL_BLEND);
	}



}