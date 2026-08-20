%:define DECL_ARRAY(name) int name<:1:> = <% 0 %>
%:define CAT(a, b) a %:%: b

int
main(void)
<%
	DECL_ARRAY(values);
	int xy = 11;

	return values<:0:> + CAT(x, y) - 11;
%>
