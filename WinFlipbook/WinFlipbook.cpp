// Include standard headers
#include <iostream>
#include <stdexcept>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <glfw3.h>

// Include GLM
#include <glm/glm.hpp>
using namespace glm;
using namespace std;

namespace {
const int NUMIMAGES = 10;

const char* vertex_prog = "\
#version 150\n                                  \
in vec2 position;                               \
void main()                                     \
{                                               \
    gl_Position = vec4(position, 0.5, 1.0);     \
}";

const char* fragment_prog = "\
#version 150\n                                  \
out vec4 outColor;                              \
uniform vec2 dims;                              \
uniform sampler2D tex;                          \
void main() {                                   \
  vec2 uv;                                      \
  uv[0] = gl_FragCoord[0]/dims[0];              \
  uv[1] = 1.0f - (gl_FragCoord[1]/dims[1]);     \
  outColor = texture(tex, uv);                  \
}                                               \
";


struct shader_base
{
protected:
	GLenum prog_type;
	GLuint shader_obj;

public:
	shader_base(const char* src, const GLenum& type)
		: prog_type(type),
		shader_obj(GL_FALSE)
	{
		shader_obj = glCreateShader(prog_type);
		glShaderSource(shader_obj, 1, &src, NULL);
		compile();
	}

	~shader_base() { glDeleteShader(shader_obj); }

	virtual void
		compile()
	{
		glCompileShader(shader_obj);

		GLint status;
		glGetShaderiv(shader_obj, GL_COMPILE_STATUS, &status);
		if (GL_TRUE != status) {
			char buffer[512];
			glGetShaderInfoLog(shader_obj, 512, NULL, buffer);
			throw runtime_error(buffer);
		}
	}

	virtual void
		attach(const GLuint program)
	{
		glAttachShader(program, shader_obj);
	}
};

struct fragment_shader: public shader_base
{
	fragment_shader(const char* src) : shader_base(src, GL_FRAGMENT_SHADER) {}
};

struct vertex_shader: public shader_base
{
	vertex_shader(const char* src) : shader_base(src, GL_VERTEX_SHADER) {}
};


struct shader_program
{
	GLuint handle;

	shader_program()
	{
		handle = glCreateProgram();
	}

	~shader_program() { glDeleteProgram(handle); }

	shader_program&
		attach(vertex_shader& vp)
	{
		vp.attach(handle);
		return *this;
	}
	shader_program&
		attach(fragment_shader& fp)
	{
		fp.attach(handle);
		glBindFragDataLocation(handle, 0, "outColor");
		return *this;
	}

	shader_program&
		link()
	{
		glLinkProgram(handle);
		return *this;
	}

	void
		use()
	{
		glUseProgram(handle);
	}

	GLint
		attrib(const char* name)
	{
		return glGetAttribLocation(handle, name);
	}

	GLint
		uniform(const char* name)
	{
		return glGetUniformLocation(handle, name);
	}
};


struct framebuffer {
	GLuint handle;
	int width, height;

	framebuffer(int _w, int _h) : width(_w), height(_h)
	{
		glGenTextures(1, &handle);
	}

	~framebuffer() { glDeleteTextures(1, &handle); }

	void
		bind()
	{
		glBindTexture(GL_TEXTURE_2D, handle);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	void
		draw(unsigned* img)
	{
		glTexImage2D(GL_TEXTURE_2D,
			0, GL_RGBA, width, height,
			0,
			GL_RGBA, GL_UNSIGNED_BYTE, img);
	}
};

}



int main(void)
{
	const int width = 1024;
	const int height = 768;
	GLFWwindow* window;

	// initialize images
	unsigned* images;
	{
		int i, x, y;
		unsigned* p;
		images = new unsigned[width * height * NUMIMAGES * 4];
		p = images;
		for (i = 0; i < NUMIMAGES; i++) {
			int bar = width * i / NUMIMAGES;
			int barw = width / NUMIMAGES;
			for (y = 0; y < height; y++) {
				for (x = 0; x < width; x++) {
					if (x >= bar && x <= bar + barw) *p++ = 0xffffffff;
					else *p++ = 0;
				}
			}
		}
	}


	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(width, height, "Flipbook", NULL, NULL);
	if (window == NULL) {
		cerr << "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials." << endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	if (glewInit() != GLEW_OK) {
		cerr << "Failed to initialize GLEW" << endl;
		glfwTerminate();
		return -1;
	}

	cout << "GL Vendor = " << glGetString(GL_VENDOR)
		<< "\nGL Renderer = " << glGetString(GL_RENDERER)
		<< "\nGL Version = " << glGetString(GL_VERSION)
		<< endl;

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	// setup vertex array object
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//
	// compile shaders
	vertex_shader vtx_shader(vertex_prog);
	fragment_shader frag_shader(fragment_prog);
	shader_program program;
	program.attach(vtx_shader)
		.attach(frag_shader)
		.link()
		.use();


	// vertex positions (X,Y)
	float vertices[] = {
		-1.f,  1.f,
		1.f,  1.f,
		1.f, -1.f,
		-1.f, -1.f,
	};

	// 2 triangles for the quad
	GLuint elements[] = {
		0, 1, 2,
		2, 3, 0
	};

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	auto pos_attr = program.attrib("position");
	glVertexAttribPointer(pos_attr, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(pos_attr);

	GLuint ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	// set image dims for the fragment shader
	auto dim_attr = program.uniform("dims");
	glUniform2f(dim_attr, float(width), float(height));

	// texture buffer
	framebuffer frame(width, height);
	frame.bind();

	// double buffer
	//glDrawBuffer(GL_BACK);

	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	int i = 0;
	do {
		//glClear(GL_COLOR_BUFFER_BIT);
		frame.draw(images + (i%NUMIMAGES)*width*height);         // send texture
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); // draw geometry

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

		++i;
	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);

	// cleanup opengl buffers
	glDeleteBuffers(1, &ebo);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

